#include <Arduino.h>
#include <WiFi.h>
#include "mbedtls/md.h"
#include "HTTPClient.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <list>
#include "mining.h"
#include "utils.h"
#include "monitor.h"
#include "drivers/storage/storage.h"
#include "drivers/devices/device.h"
#ifdef NERD_NOS
#include "mining_guardian_max.h"
#include "drivers/guardian-max/adc.h"
#endif

extern uint32_t templates;
extern uint32_t hashes;
extern uint32_t Mhashes;
extern uint32_t totalKHashes;
extern uint32_t elapsedKHs;
extern uint64_t upTime;

extern uint32_t shares; // increase if blockhash has 32 bits of zeroes
extern uint32_t valids; // increased if blockhash <= targethalfshares

extern double best_diff; // track best diff

extern monitor_data mMonitor;

//from saved config
extern TSettings Settings; 
bool invertColors = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
unsigned int bitcoin_price=0;
static bool s_cacheOnly = false;
static bool s_freezeHashrate = false;

void monitorSetCacheOnly(bool on)
{
  s_cacheOnly = on;
}

void monitorFreezeHashrate(bool on)
{
  s_freezeHashrate = on;
}

static unsigned long mFeeUpdate = 0;

#ifdef GUARDIAN
void updateGlobalData(void);
String getBlockHeight(void);
bool guardianFetchNetwork(void);
void getTime(unsigned long *currentHours, unsigned long *currentMinutes, unsigned long *currentSeconds);

static void guardianNetTask(void *)
{
  vTaskDelay(1200 / portTICK_PERIOD_MS);
  int spins = 0;
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (!guardianFetchNetwork())
      {
        getBTCprice();
        getBlockHeight();
        updateGlobalData();
      }
      else if (mFeeUpdate == 0)
        updateGlobalData();
      unsigned long hours, minutes, seconds;
      getTime(&hours, &minutes, &seconds);
    }
    spins++;
    uint32_t waitMs = (spins < 8) ? 5000 : 20000;
    vTaskDelay(waitMs / portTICK_PERIOD_MS);
  }
}
#endif

void monitorStartNetworkTask(void)
{
#ifdef GUARDIAN
  xTaskCreatePinnedToCore(guardianNetTask, "gNet", 16384, NULL, 4, NULL, 0);
#endif
}
#ifdef GUARDIAN
String current_block = "";
#else
String current_block = "793261";
#endif
global_data gData;
pool_data pData;
String poolAPIUrl;


void setup_monitor(void){
    /******** TIME ZONE SETTING *****/

    timeClient.begin();
    
    // Adjust offset depending on your zone
    // GMT +2 in seconds (zona horaria de Europa Central)
    timeClient.setTimeOffset(3600 * Settings.Timezone);

    Serial.println("TimeClient setup done");
#ifdef GUARDIAN
    gData.halfHourFee = -1;
#endif
#ifdef SCREEN_WORKERS_ENABLE
    poolAPIUrl = getPoolAPIUrl();
    Serial.println("poolAPIUrl: " + poolAPIUrl);
#endif
}

unsigned long mGlobalUpdate =0;

static void beginHttp(HTTPClient &http, const char *url)
{
#ifdef GUARDIAN
    http.setTimeout(4000);
    http.setUserAgent("Mozilla/5.0");
#else
    http.setTimeout(10000);
#endif
    http.begin(url);
}

static void storeGlobalHash(double hashrate)
{
    if (!(hashrate > 1e15))
        return;
    double eh = hashrate / 1e18;
    char buf[16];
    if (eh >= 100.0)
        snprintf(buf, sizeof(buf), "%.0f", eh);
    else
        snprintf(buf, sizeof(buf), "%.1f", eh);
    gData.globalHash = buf;
}

static void storeDifficulty(double difficulty)
{
    if (!(difficulty > 1e9))
        return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2fT", difficulty / 1e12);
    gData.difficulty = buf;
}

void updateGlobalData(void){
    
    if (s_cacheOnly || WiFi.status() != WL_CONNECTED)
        return;

    bool needHash = (mGlobalUpdate == 0) || (millis() - mGlobalUpdate > UPDATE_Global_min * 60 * 1000);
    bool needFee = (mFeeUpdate == 0) || (millis() - mFeeUpdate > UPDATE_Global_min * 60 * 1000);
    if (!needHash && !needFee)
        return;

    HTTPClient http;
    try {
    if (needHash) {
        beginHttp(http, getGlobalHash);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            StaticJsonDocument<1024> doc;
            deserializeJson(doc, payload);
            if (doc.containsKey("currentHashrate"))
                storeGlobalHash(doc["currentHashrate"].as<double>());
            if (doc.containsKey("currentDifficulty"))
                storeDifficulty(doc["currentDifficulty"].as<double>());
            doc.clear();

            if (gData.globalHash.length() && gData.difficulty.length()) {
                mGlobalUpdate = millis();
                Serial.printf("[NET] hash %s EH/s diff %s\n", gData.globalHash.c_str(), gData.difficulty.c_str());
            }
        }
        http.end();
    }

    if (needFee) {
        beginHttp(http, getFees);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            StaticJsonDocument<1024> doc;
            deserializeJson(doc, payload);
            if (doc.containsKey("halfHourFee")) gData.halfHourFee = doc["halfHourFee"].as<int>();
#ifdef SCREEN_FEES_ENABLE
            if (doc.containsKey("fastestFee"))  gData.fastestFee = doc["fastestFee"].as<int>();
            if (doc.containsKey("hourFee"))     gData.hourFee = doc["hourFee"].as<int>();
            if (doc.containsKey("economyFee"))  gData.economyFee = doc["economyFee"].as<int>();
            if (doc.containsKey("minimumFee"))  gData.minimumFee = doc["minimumFee"].as<int>();
#endif
            doc.clear();

            if (gData.halfHourFee >= 0) {
                mFeeUpdate = millis();
                Serial.printf("[NET] fee %d sat/vB\n", gData.halfHourFee);
            }
        }
        
        http.end();
    }
    } catch(...) {
      Serial.println("Global data HTTP error caught");
      http.end();
    }
}

unsigned long mHeightUpdate = 0;

static bool digitsOnly(const String &text)
{
    if (!text.length())
        return false;
    for (unsigned i = 0; i < text.length(); ++i) {
        char c = text.charAt(i);
        if (c < '0' || c > '9')
            return false;
    }
    return true;
}

String getBlockHeight(void){
    
    if(!s_cacheOnly && ((mHeightUpdate == 0) || (millis() - mHeightUpdate > UPDATE_Height_min * 60 * 1000))){
    
        if (WiFi.status() != WL_CONNECTED) return current_block;
            
        HTTPClient http;
        try {
        beginHttp(http, getHeightAPI);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            payload.trim();

            if (digitsOnly(payload) && payload.toInt() > 100000) {
                current_block = payload;
                mHeightUpdate = millis();
                Serial.printf("[NET] height %s\n", current_block.c_str());
            }
        }        
        http.end();
        } catch(...) {
          Serial.println("Height HTTP error caught");
          http.end();
        }
    }
  
  return current_block;
}

unsigned long mBTCUpdate = 0;

static String formatBtcPrice(void)
{
#ifdef GUARDIAN
    if (mBTCUpdate == 0)
        return String("--");
#endif
    static char price_buffer[16];
    snprintf(price_buffer, sizeof(price_buffer), "$%u", bitcoin_price);
    return String(price_buffer);
}

String getBTCprice(void){
    
    if(!s_cacheOnly && ((mBTCUpdate == 0) || (millis() - mBTCUpdate > UPDATE_BTC_min * 60 * 1000))){
    
        if (WiFi.status() != WL_CONNECTED)
            return formatBtcPrice();
        
        HTTPClient http;

        try {
        beginHttp(http, getBTCAPI);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            StaticJsonDocument<1024> doc;
            deserializeJson(doc, payload);
          
            if (doc.containsKey("bitcoin") && doc["bitcoin"].containsKey("usd")) {
                unsigned int usd = doc["bitcoin"]["usd"].as<unsigned int>();
                if (usd > 0) {
                    bitcoin_price = usd;
                    mBTCUpdate = millis();
                    Serial.printf("[NET] price $%u\n", bitcoin_price);
                }
            }

            doc.clear();
        }
        
        http.end();
        } catch(...) {
          Serial.println("BTC price HTTP error caught");
          http.end();
        }
    }  
  
  return formatBtcPrice();
}

#ifdef GUARDIAN
static bool guardianNetworkFresh(void)
{
  if (mBTCUpdate == 0 || mHeightUpdate == 0 || mGlobalUpdate == 0 || mFeeUpdate == 0)
    return false;
  unsigned long now = millis();
  if (now - mBTCUpdate > UPDATE_BTC_min * 60UL * 1000UL)
    return false;
  if (now - mHeightUpdate > UPDATE_Height_min * 60UL * 1000UL)
    return false;
  if (now - mGlobalUpdate > UPDATE_Global_min * 60UL * 1000UL)
    return false;
  if (now - mFeeUpdate > UPDATE_Global_min * 60UL * 1000UL)
    return false;
  return true;
}

bool guardianFetchNetwork(void)
{
  if (s_cacheOnly || WiFi.status() != WL_CONNECTED)
    return false;
  if (guardianNetworkFresh())
    return true;

  HTTPClient http;
  bool applied = false;
  try {
    beginHttp(http, getGuardianAPI);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<768> doc;
      if (!deserializeJson(doc, payload)) {
        if (doc.containsKey("btc_usd") && !doc["btc_usd"].isNull()) {
          unsigned int usd = doc["btc_usd"].as<unsigned int>();
          if (usd > 1000) {
            bitcoin_price = usd;
            mBTCUpdate = millis();
            applied = true;
            Serial.printf("[NET] price $%u\n", bitcoin_price);
          }
        }
        if (doc.containsKey("block_height") && !doc["block_height"].isNull()) {
          unsigned long height = doc["block_height"].as<unsigned long>();
          if (height > 100000) {
            current_block = String(height);
            mHeightUpdate = millis();
            applied = true;
            Serial.printf("[NET] height %s\n", current_block.c_str());
          }
        }
        if (doc.containsKey("nethash_ehs") && !doc["nethash_ehs"].isNull()) {
          double eh = doc["nethash_ehs"].as<double>();
          if (eh > 1.0) {
            char buf[16];
            if (eh >= 100.0)
              snprintf(buf, sizeof(buf), "%.0f", eh);
            else
              snprintf(buf, sizeof(buf), "%.1f", eh);
            gData.globalHash = buf;
          }
        }
        if (doc.containsKey("difficulty") && !doc["difficulty"].isNull())
          storeDifficulty(doc["difficulty"].as<double>());
        if (gData.globalHash.length() && gData.difficulty.length()) {
          mGlobalUpdate = millis();
          applied = true;
          Serial.printf("[NET] hash %s EH/s diff %s\n", gData.globalHash.c_str(), gData.difficulty.c_str());
        }
        if (doc.containsKey("fee_sat_vb") && !doc["fee_sat_vb"].isNull()) {
          int fee = doc["fee_sat_vb"].as<int>();
          if (fee >= 0) {
            gData.halfHourFee = fee;
            mFeeUpdate = millis();
            applied = true;
            Serial.printf("[NET] fee %d sat/vB\n", fee);
          }
        }
      }
    }
    http.end();
  } catch (...) {
    Serial.println("Guardian API HTTP error caught");
    http.end();
    applied = false;
  }
  return applied;
}
#endif

unsigned long mTriggerUpdate = 0;
unsigned long initialMillis = millis();
unsigned long initialTime = 0;
unsigned long mPoolUpdate = 0;

void getTime(unsigned long* currentHours, unsigned long* currentMinutes, unsigned long* currentSeconds){
  
  //Check if need an NTP call to check current time
  if(!s_cacheOnly && ((mTriggerUpdate == 0) || (millis() - mTriggerUpdate > UPDATE_PERIOD_h * 60 * 60 * 1000))){ //60 sec. * 60 min * 1000ms
    if(WiFi.status() == WL_CONNECTED) {
        if(timeClient.update()) mTriggerUpdate = millis(); //NTP call to get current time
        initialTime = timeClient.getEpochTime(); // Guarda la hora inicial (en segundos desde 1970)
        Serial.print("TimeClient NTPupdateTime ");
    }
  }

  unsigned long elapsedTime = (millis() - mTriggerUpdate) / 1000; // Tiempo transcurrido en segundos
  unsigned long currentTime = initialTime + elapsedTime; // La hora actual

  // convierte la hora actual en horas, minutos y segundos
  *currentHours = currentTime % 86400 / 3600;
  *currentMinutes = currentTime % 3600 / 60;
  *currentSeconds = currentTime % 60;
}

String getDate(){
  
  unsigned long elapsedTime = (millis() - mTriggerUpdate) / 1000; // Tiempo transcurrido en segundos
  unsigned long currentTime = initialTime + elapsedTime; // La hora actual

  // Convierte la hora actual (epoch time) en una estructura tm
  struct tm *tm = localtime((time_t *)&currentTime);

  int year = tm->tm_year + 1900; // tm_year es el número de años desde 1900
  int month = tm->tm_mon + 1;    // tm_mon es el mes del año desde 0 (enero) hasta 11 (diciembre)
  int day = tm->tm_mday;         // tm_mday es el día del mes

  char currentDate[20];
  sprintf(currentDate, "%02d/%02d/%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);

  return String(currentDate);
}

String getTime(void){
#ifdef GUARDIAN
  if (mTriggerUpdate == 0)
    return String("--:--");
#endif
  unsigned long currentHours, currentMinutes, currentSeconds;
  getTime(&currentHours, &currentMinutes, &currentSeconds);

  char LocalHour[10];
  sprintf(LocalHour, "%02d:%02d", currentHours, currentMinutes);
  
  String mystring(LocalHour);
  return LocalHour;
}

enum EHashRateScale
{
  HashRateScale_99KH,
  HashRateScale_999KH,
  HashRateScale_9MH
};

static EHashRateScale s_hashrate_scale = HashRateScale_99KH;
static uint32_t s_skip_first = 3;
static double s_top_hashrate = 0.0;

static std::list<double> s_hashrate_avg_list;
static double s_hashrate_summ = 0.0;
static uint8_t s_hashrate_recalc = 0;

static String formatAvgHashrate(double avg_hashrate)
{
  if (avg_hashrate < 0.0)
    avg_hashrate = 0.0;

  switch (s_hashrate_scale)
  {
    case HashRateScale_99KH:
      return String(avg_hashrate, 2);
    case HashRateScale_999KH:
      return String(avg_hashrate, 1);
    default:
      return String((int)avg_hashrate);
  }
}

String getCurrentHashRate(unsigned long mElapsed)
{
#ifdef NERD_NOS
  return String(nerdnos_get_avg_hashrate(), 1);
#else
  if (s_freezeHashrate)
  {
    double avg = 0.0;
    if (!s_hashrate_avg_list.empty())
      avg = s_hashrate_summ / (double)s_hashrate_avg_list.size();
    return formatAvgHashrate(avg);
  }

  if (mElapsed == 0)
    mElapsed = 1;

  double hashrate = (double)elapsedKHs * 1000.0 / (double)mElapsed;

  s_hashrate_summ += hashrate;
  s_hashrate_avg_list.push_back(hashrate);
  if (s_hashrate_avg_list.size() > 10)
  {
    s_hashrate_summ -= s_hashrate_avg_list.front();
    s_hashrate_avg_list.pop_front();
  }

  ++s_hashrate_recalc;
  if (s_hashrate_recalc == 0)
  {
    s_hashrate_summ = 0.0;
    for (auto itt = s_hashrate_avg_list.begin(); itt != s_hashrate_avg_list.end(); ++itt)
      s_hashrate_summ += *itt;
  }

  double avg_hashrate = s_hashrate_summ / (double)s_hashrate_avg_list.size();
  if (avg_hashrate < 0.0)
    avg_hashrate = 0.0;

  if (s_skip_first > 0)
  {
    s_skip_first--;
  } else
  {
    if (avg_hashrate > s_top_hashrate)
    {
      s_top_hashrate = avg_hashrate;
      if (avg_hashrate > 999.9)
        s_hashrate_scale = HashRateScale_9MH;
      else if (avg_hashrate > 99.9)
        s_hashrate_scale = HashRateScale_999KH;
    }
  }

  return formatAvgHashrate(avg_hashrate);
#endif
}

mining_data getMiningData(unsigned long mElapsed)
{
  mining_data data;

  char best_diff_string[16] = {0};
  suffix_string(best_diff, best_diff_string, 16, 0);

  char timeMining[15] = {0};
  uint64_t tm = upTime;
  int secs = tm % 60;
  tm /= 60;
  int mins = tm % 60;
  tm /= 60;
  int hours = tm % 24;
  int days = tm / 24;
  sprintf(timeMining, "%01d  %02d:%02d:%02d", days, hours, mins, secs);

  data.completedShares = shares;
#ifdef NERD_NOS
  // ASIC work estimated from accepted shares (diff * 2^32); the ESP does not hash.
  data.totalMHashes = String(nerdnos_get_total_mhashes(), 0);
#else
  data.totalMHashes = Mhashes;
#endif
  data.totalKHashes = totalKHashes;
  data.currentHashRate = getCurrentHashRate(mElapsed);
  data.templates = templates;
  data.bestDiff = best_diff_string;
  data.timeMining = timeMining;
  data.valids = valids;
#ifdef NERD_NOS
  data.temp = String(nerdnos_get_temperature(), 0);
#else
  data.temp = String(temperatureRead(), 0);
#endif
  data.currentTime = getTime();
  char poolBuf[16];
  snprintf(poolBuf, sizeof(poolBuf), "%.4g", currentPoolDifficulty);
  data.poolDiff = poolBuf;

  return data;
}

clock_data getClockData(unsigned long mElapsed)
{
  clock_data data;

  data.completedShares = shares;
  data.totalKHashes = totalKHashes;
  data.currentHashRate = getCurrentHashRate(mElapsed);
  data.btcPrice = getBTCprice();
  data.blockHeight = getBlockHeight();
  data.currentTime = getTime();
  data.currentDate = getDate();

  return data;
}

clock_data_t getClockData_t(unsigned long mElapsed)
{
  clock_data_t data;

  data.valids = valids;
  data.currentHashRate = getCurrentHashRate(mElapsed);
  getTime(&data.currentHours, &data.currentMinutes, &data.currentSeconds);

  return data;
}

coin_data getCoinData(unsigned long mElapsed)
{
  coin_data data;

  updateGlobalData(); // Update gData vars asking mempool APIs

  data.completedShares = shares;
  data.totalKHashes = totalKHashes;
  data.currentHashRate = getCurrentHashRate(mElapsed);
  data.btcPrice = getBTCprice();
  data.currentTime = getTime();
#ifdef SCREEN_FEES_ENABLE
  data.hourFee = String(gData.hourFee);
  data.fastestFee = String(gData.fastestFee);
  data.economyFee = String(gData.economyFee);
  data.minimumFee = String(gData.minimumFee);
#endif
#ifdef GUARDIAN
  if (gData.halfHourFee >= 0)
    data.halfHourFee = String(gData.halfHourFee) + " sat/vB";
  else
    data.halfHourFee = "";
#else
  data.halfHourFee = String(gData.halfHourFee) + " sat/vB";
#endif
  data.netwrokDifficulty = gData.difficulty;
  data.globalHashRate = gData.globalHash;
  data.blockHeight = getBlockHeight();

  unsigned long currentBlock = data.blockHeight.toInt();
  if (currentBlock >= 100000) {
    unsigned long remainingBlocks = (((currentBlock / HALVING_BLOCKS) + 1) * HALVING_BLOCKS) - currentBlock;
    data.progressPercent = (HALVING_BLOCKS - remainingBlocks) * 100 / HALVING_BLOCKS;
    data.remainingBlocks = String(remainingBlocks) + " BLOCKS";
  } else {
    data.progressPercent = 0;
    data.remainingBlocks = "";
  }

  return data;
}

String getPoolAPIUrl(void) {
    poolAPIUrl = String(getPublicPool);
    if (Settings.PoolAddress == "public-pool.io") {
        poolAPIUrl = "https://public-pool.io:40557/api/client/";
    } 
    else {
        if (Settings.PoolAddress == "pool.nerdminers.org") {
            poolAPIUrl = "https://pool.nerdminers.org/users/";
        }
        else {
            switch (Settings.PoolPort) {
                case 3333:
                    if (Settings.PoolAddress == "pool.sethforprivacy.com")
                        poolAPIUrl = "https://pool.sethforprivacy.com/api/client/";
                    if (Settings.PoolAddress == "pool.solomining.de")
                        poolAPIUrl = "https://pool.solomining.de/api/client/";
                    // Add more cases for other addresses with port 3333 if needed
                    break;
                case 2018:
                    // Local instance of public-pool.io on Umbrel or Start9
                    poolAPIUrl = "http://" + Settings.PoolAddress + ":2019/api/client/";
                    break;
                default:
                    poolAPIUrl = String(getPublicPool);
                    break;
            }
        }
    }
    return poolAPIUrl;
}

pool_data getPoolData(void){
    //pool_data pData;    
    if((mPoolUpdate == 0) || (millis() - mPoolUpdate > UPDATE_POOL_min * 60 * 1000)){      
        if (WiFi.status() != WL_CONNECTED) return pData;            
        //Make first API call to get global hash and current difficulty
        HTTPClient http;
        http.setTimeout(10000);        
        try {          
          String btcWallet = Settings.BtcWallet;
          // Serial.println(btcWallet);
          if (btcWallet.indexOf(".")>0) btcWallet = btcWallet.substring(0,btcWallet.indexOf("."));
#ifdef SCREEN_WORKERS_ENABLE
          Serial.println("Pool API : " + poolAPIUrl+btcWallet);
          http.begin(poolAPIUrl+btcWallet);
#else
          http.begin(String(getPublicPool)+btcWallet);
#endif
          int httpCode = http.GET();
          if (httpCode == HTTP_CODE_OK) {
              String payload = http.getString();
              // Serial.println(payload);
              StaticJsonDocument<300> filter;
              filter["bestDifficulty"] = true;
              filter["workersCount"] = true;
              filter["workers"][0]["sessionId"] = true;
              filter["workers"][0]["hashRate"] = true;
              StaticJsonDocument<2048> doc;
              deserializeJson(doc, payload, DeserializationOption::Filter(filter));
              //Serial.println(serializeJsonPretty(doc, Serial));
              if (doc.containsKey("workersCount")) pData.workersCount = doc["workersCount"].as<int>();
              const JsonArray& workers = doc["workers"].as<JsonArray>();
              float totalhashs = 0;
              for (const JsonObject& worker : workers) {
                totalhashs += worker["hashRate"].as<double>();
                /* Serial.print(worker["sessionId"].as<String>()+": ");
                Serial.print(" - "+worker["hashRate"].as<String>()+": ");
                Serial.println(totalhashs); */
              }
              char totalhashs_s[16] = {0};
              suffix_string(totalhashs, totalhashs_s, 16, 0);
              pData.workersHash = String(totalhashs_s);

              double temp;
              if (doc.containsKey("bestDifficulty")) {
              temp = doc["bestDifficulty"].as<double>();            
              char best_diff_string[16] = {0};
              suffix_string(temp, best_diff_string, 16, 0);
              pData.bestDifficulty = String(best_diff_string);
              }
              doc.clear();
              mPoolUpdate = millis();
              Serial.println("\n####### Pool Data OK!");               
          } else {
              Serial.println("\n####### Pool Data HTTP Error!");    
              /* Serial.println(httpCode);
              String payload = http.getString();
              Serial.println(payload); */
              // mPoolUpdate = millis();
              pData.bestDifficulty = "P";
              pData.workersHash = "E";
              pData.workersCount = 0;
              http.end();
              return pData; 
          }
          http.end();
        } catch(...) {
          Serial.println("####### Pool Error!");          
          // mPoolUpdate = millis();
          pData.bestDifficulty = "P";
          pData.workersHash = "Error";
          pData.workersCount = 0;
          http.end();
          return pData;
        } 
    }
    return pData;
}
