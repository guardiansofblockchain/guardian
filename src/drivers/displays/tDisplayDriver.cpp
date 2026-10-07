#include "displayDriver.h"

#ifdef T_DISPLAY

#include <TFT_eSPI.h>
#include "media/images_320_170.h"
#include "media/myFonts.h"
#include "media/Free_Fonts.h"
#include "version.h"
#include "monitor.h"
#include "OpenFontRender.h"
#include "rotation.h"
#ifdef GUARDIAN
#include "media/images_guardian_320_170.h"
#include "media/guardian_miner_bg.h"
#include "media/guardian_cycle_bg.h"
#ifdef NERD_NOS
#include "media/guardian_max_bg.h"
#endif
#if __has_include("media/DMSans_subset.h")
#include "media/DMSans_subset.h"
#define GUARDIAN_USE_DM_SANS
#endif
#endif

#define WIDTH 340
#define HEIGHT 170

OpenFontRender render;
#ifdef GUARDIAN_USE_DM_SANS
OpenFontRender renderBold;
static bool guardianBoldReady = false;
#endif
TFT_eSPI tft = TFT_eSPI();                  // Invoke library, pins defined in User_Setup.h
TFT_eSprite background = TFT_eSprite(&tft); // Invoke library sprite

void tDisplay_Init(void)
{
      //Init pin 15 to eneble 5V external power (LilyGo bug)
#ifdef PIN_ENABLE5V
    pinMode(PIN_ENABLE5V, OUTPUT);
    digitalWrite(PIN_ENABLE5V, HIGH);
#endif
  
  tft.init();
  #ifdef LILYGO_S3_T_EMBED
  tft.setRotation(ROTATION_270);
  #else
  tft.setRotation(ROTATION_90);
  #endif
  tft.setSwapBytes(true);                 // Swap the colour byte order when rendering
  background.createSprite(WIDTH, HEIGHT); // Background Sprite
  background.setSwapBytes(true);
  render.setDrawer(background);  // Link drawing object to background instance (so font will be rendered on background)
  render.setLineSpaceRatio(0.9); // Espaciado entre texto
  render.setBackgroundFillMethod(BgFillMethod::None);

  // Load the font and check it can be read OK
#ifdef GUARDIAN_USE_DM_SANS
  if (render.loadFont(DMSans_Regular_subset, DMSans_Regular_subset_SIZE))
#else
  if (render.loadFont(DigitalNumbers, sizeof(DigitalNumbers)))
#endif
  {
    Serial.println("Initialise error");
    return;
  }
#ifdef GUARDIAN_USE_DM_SANS
  renderBold.setDrawer(background);
  renderBold.setLineSpaceRatio(0.9);
  renderBold.setBackgroundFillMethod(BgFillMethod::None);
  guardianBoldReady = renderBold.loadFont(DMSans_Bold_subset, DMSans_Bold_subset_SIZE) == 0;
  if (!guardianBoldReady)
    Serial.println("Bold font failed, falling back to regular");
#endif
}

#ifdef GUARDIAN
#ifdef GUARDIAN_USE_DM_SANS
static OpenFontRender &guardianFont(bool bold)
{
  return (bold && guardianBoldReady) ? renderBold : render;
}
#else
static OpenFontRender &guardianFont(bool)
{
  return render;
}
#endif
#endif

void tDisplay_AlternateScreenState(void)
{
  int screen_state = digitalRead(TFT_BL);
  Serial.println("Switching display state");
  digitalWrite(TFT_BL, !screen_state);
}

void tDisplay_AlternateRotation(void)
{
  tft.setRotation( flipRotation(tft.getRotation()) );
}

#ifdef GUARDIAN
static String guardianNumber(const String &src)
{
  String out;
  for (unsigned i = 0; i < src.length(); ++i)
  {
    char c = src.charAt(i);
    if ((c >= '0' && c <= '9') || c == '.' || c == ',')
      out += c;
    else if (out.length() > 0)
      break;
  }
  return out;
}

static String guardianGrouped(const String &src)
{
  String n = guardianNumber(src);
  String out;
  int count = 0;
  for (int i = (int)n.length() - 1; i >= 0; --i)
  {
    if (count > 0 && (count % 3) == 0)
      out = String(" ") + out;
    out = String(n.charAt(i)) + out;
    count++;
  }
  return out;
}

static const uint16_t gWhite = 0xFFFF;
static const uint16_t gPrice = 0xD6BB;
static const uint16_t gMuted = 0x9D15;  // #99a1af
static const uint16_t gCyan = 0x0739;   // #00e5cf
static const uint16_t gInk = 0x10A4;    // #121523
static const uint16_t gBg = 0x1082;
static const uint16_t gCard = 0x18E4;
static const uint16_t gYellow = 0xFDE0; // #ffbf00
static const uint16_t gPill = 0x05D4;   // #02b9a7

static void guardianPillPrice(const char *price)
{
  String grouped = guardianGrouped(price ? price : "");
  OpenFontRender &pen = guardianFont(true);
  // Pill interior is y 11..28, text ends before the status dot at x=281.
  if (!grouped.length() || grouped == "0")
  {
    pen.setFontSize(13);
    pen.rdrawString("--", 274, 12, gPrice, gCard);
    return;
  }
  String line = grouped + " USD";
  // 13pt bold fits "999 999 USD"; longer prices drop a step so they stay clear of the icon
  pen.setFontSize(grouped.length() > 7 ? 11 : 13);
  pen.rdrawString(line.c_str(), 274, 12, gPrice, gCard);
}

// Total hashes since the stats were last reset, given in megahashes.
// Picks a unit so the number stays short: 18 420 MH, 21.9 GH, 720.5 TH, 1.2 PH.
static String guardianTotalHashes(const String &megaHashes)
{
  double mh = atof(guardianNumber(megaHashes).c_str());
  if (mh < 0)
    mh = 0;
  if (mh < 100000.0)
    return guardianGrouped(String((unsigned long)(mh + 0.5))) + " MH";
  const char *units[] = {"GH", "TH", "PH", "EH"};
  double value = mh / 1000.0;
  int unit = 0;
  while (value >= 1000.0 && unit < 3)
  {
    value /= 1000.0;
    unit++;
  }
  return String(value, value < 100.0 ? 1 : 0) + " " + units[unit];
}

static const char *guardianOrDash(const char *value)
{
  return (value && value[0]) ? value : "--";
}

static void guardianBottomBars(const char *hashRate, const char *blockHeight)
{
  render.setFontSize(16);
  render.cdrawString(guardianOrDash(hashRate), 82, 134, gInk, gYellow);
  render.setFontSize(9);
#ifdef NERD_NOS
  render.cdrawString("GH/s", 122, 140, gInk, gYellow);
#else
  render.cdrawString("KH/s", 122, 140, gInk, gYellow);
#endif

  render.setFontSize(16);
  render.cdrawString(guardianOrDash(blockHeight), 222, 134, gInk, gCyan);
  render.setFontSize(7);
  render.cdrawString("CURRENT", 272, 135, gInk, gCyan);
  render.cdrawString("BLOCK", 272, 144, gInk, gCyan);
}
#endif

void tDisplay_MinerScreen(unsigned long mElapsed)
{
  mining_data data = getMiningData(mElapsed);

#ifdef GUARDIAN
  background.fillSprite(0x1082);
#ifdef NERD_NOS
  background.pushImage(0, 0, guardianMaxMinerWidth, guardianMaxMinerHeight, guardianMaxMinerScreen);
#else
  background.pushImage(0, 0, guardianMinerWidth, guardianMinerHeight, guardianMinerScreen);
#endif

  String price = getBTCprice();
  guardianPillPrice(price.c_str());

  render.setFontSize(8);
  render.cdrawString("CURRENT HASHRATE", 160, 50, gMuted, gBg);

  OpenFontRender &rate = guardianFont(true);
  rate.setFontSize(36);
  rate.cdrawString(data.currentHashRate.c_str(), 152, 60, gWhite, gBg);

  render.setFontSize(7);
  String total = guardianTotalHashes(data.totalMHashes);
  render.cdrawString(total.c_str(), 160, 102, gMuted, gBg);

  render.setFontSize(9);
  render.cdrawString(data.valids.c_str(), 45, 139, gWhite, gCard);
  render.cdrawString(data.bestDiff.c_str(), 122, 139, gWhite, gCard);
  String tempLabel = data.temp + "\xC2\xB0";
  render.cdrawString(tempLabel.c_str(), 199, 139, gWhite, gCard);
  render.cdrawString(data.completedShares.c_str(), 276, 139, gWhite, gCard);

  render.setFontSize(7);
  render.cdrawString("BLOCKS", 45, 151, gMuted, gCard);
  render.cdrawString("DIFF", 122, 151, gMuted, gCard);
  render.cdrawString("TEMP", 199, 151, gMuted, gCard);
  render.cdrawString("SHARES", 276, 151, gMuted, gCard);
#else
  // Print background screen
  background.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());

  // Hashrate
  render.setFontSize(35);
  render.setCursor(19, 118);
  render.setFontColor(TFT_BLACK);

  render.rdrawString(data.currentHashRate.c_str(), 118, 114, TFT_BLACK);
  // Total hashes
  render.setFontSize(18);
  render.rdrawString(data.totalMHashes.c_str(), 268, 138, TFT_BLACK);
  // Block templates
  render.setFontSize(18);
  render.drawString(data.templates.c_str(), 186, 20, 0xDEDB);
  // Best diff
  render.drawString(data.bestDiff.c_str(), 186, 48, 0xDEDB);
  // 32Bit shares
  render.setFontSize(18);
  render.drawString(data.completedShares.c_str(), 186, 76, 0xDEDB);
  // Hores
  render.setFontSize(14);
  render.rdrawString(data.timeMining.c_str(), 315, 104, 0xDEDB);

  // Valid Blocks
  render.setFontSize(24);
  render.drawString(data.valids.c_str(), 285, 56, 0xDEDB);

  // Print Temp
  render.setFontSize(10);
  render.rdrawString(data.temp.c_str(), 239, 1, TFT_BLACK);

  render.setFontSize(4);
  render.rdrawString(String(0).c_str(), 244, 3, TFT_BLACK);

  // Print Hour
  render.setFontSize(10);
  render.rdrawString(data.currentTime.c_str(), 286, 1, TFT_BLACK);
#endif

  // Push prepared background to screen
  background.pushSprite(0, 0);
}

void tDisplay_ClockScreen(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

#ifdef GUARDIAN
  background.fillSprite(0x1082);
#ifdef NERD_NOS
  background.pushImage(0, 0, guardianMaxClockWidth, guardianMaxClockHeight, guardianMaxClockScreen);
#else
  background.pushImage(0, 0, guardianClockWidth, guardianClockHeight, guardianClockScreen);
#endif
  guardianPillPrice(data.btcPrice.c_str());

  render.setFontSize(8);
  render.cdrawString("CURRENT TIME", 160, 50, gMuted, gBg);
  OpenFontRender &clockPen = guardianFont(true);
  clockPen.setFontSize(40);
  clockPen.cdrawString(data.currentTime.c_str(), 160, 60, gWhite, gBg);
  guardianBottomBars(data.currentHashRate.c_str(), data.blockHeight.c_str());
#else
  // Print background screen
  background.pushImage(0, 0, minerClockWidth, minerClockHeight, minerClockScreen);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());

  // Hashrate
  render.setFontSize(25);
  render.setCursor(19, 122);
  render.setFontColor(TFT_BLACK);
  render.rdrawString(data.currentHashRate.c_str(), 94, 129, TFT_BLACK);

  // Print BTC Price
  background.setFreeFont(FSSB9);
  background.setTextSize(1);
  background.setTextDatum(TL_DATUM);
  background.setTextColor(TFT_BLACK);
  background.drawString(data.btcPrice.c_str(), 202, 3, GFXFF);

  // Print BlockHeight
  render.setFontSize(18);
  render.rdrawString(data.blockHeight.c_str(), 254, 140, TFT_BLACK);

  // Print Hour
  background.setFreeFont(FF23);
  background.setTextSize(2);
  background.setTextColor(0xDEDB, TFT_BLACK);

  background.drawString(data.currentTime.c_str(), 130, 50, GFXFF);
#endif

  // Push prepared background to screen
  background.pushSprite(0, 0);
}

void tDisplay_GlobalHashScreen(unsigned long mElapsed)
{
  coin_data data = getCoinData(mElapsed);

#ifdef GUARDIAN
  background.fillSprite(0x1082);
#ifdef NERD_NOS
  background.pushImage(0, 0, guardianMaxGlobalWidth, guardianMaxGlobalHeight, guardianMaxGlobalScreen);
#else
  background.pushImage(0, 0, guardianGlobalWidth, guardianGlobalHeight, guardianGlobalScreen);
#endif
  guardianPillPrice(data.btcPrice.c_str());

  String difficulty = guardianNumber(data.netwrokDifficulty);
  String fee = guardianNumber(data.halfHourFee);
  String left = guardianNumber(data.remainingBlocks);

  if (difficulty.length())
  {
    render.setFontSize(18);
    render.rdrawString(difficulty.c_str(), 78, 68, gWhite, gBg);
    render.setFontSize(12);
    render.cdrawString("T", 90, 74, gCyan, gBg);
  }

  if (fee.length())
  {
    render.setFontSize(18);
    render.rdrawString(fee.c_str(), 256, 68, gWhite, gBg);
    render.setFontSize(9);
    render.cdrawString("sat/vB", 276, 74, gCyan, gBg);
  }

  render.setFontSize(16);
  render.cdrawString(guardianOrDash(data.blockHeight.c_str()), 43, 125, gInk, gCyan);
  render.setFontSize(7);
  render.cdrawString("CURRENT", 96, 124, gInk, gCyan);
  render.cdrawString("BLOCK", 96, 134, gInk, gCyan);

  if (left.length())
  {
    char leftLine[28];
    snprintf(leftLine, sizeof(leftLine), "%s BLOCKS LEFT", left.c_str());
    render.setFontSize(7);
    render.cdrawString(leftLine, 56, 147, gWhite, gPill);
  }

  render.setFontSize(8);
  render.cdrawString("HALVING", 160, 134, gWhite, gInk);

  if (data.globalHashRate.length())
  {
    render.setFontSize(16);
    render.rdrawString(data.globalHashRate.c_str(), 230, 128, gInk, gYellow);
    render.setFontSize(8);
    render.cdrawString("EH/s", 248, 134, gInk, gYellow);
  }
  render.setFontSize(7);
  render.cdrawString("GLOBAL", 286, 126, gInk, gYellow);
  render.cdrawString("HASHRATE", 286, 136, gInk, gYellow);
#else
  // Print background screen
  background.pushImage(0, 0, globalHashWidth, globalHashHeight, globalHashScreen);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());

  // Print BTC Price
  background.setFreeFont(FSSB9);
  background.setTextSize(1);
  background.setTextDatum(TL_DATUM);
  background.setTextColor(TFT_BLACK);
  background.drawString(data.btcPrice.c_str(), 198, 3, GFXFF);

  // Print Hour
  background.setFreeFont(FSSB9);
  background.setTextSize(1);
  background.setTextDatum(TL_DATUM);
  background.setTextColor(TFT_BLACK);
  background.drawString(data.currentTime.c_str(), 268, 3, GFXFF);

  // Print Last Pool Block
  background.setFreeFont(FSS9);
  background.setTextDatum(TR_DATUM);
  background.setTextColor(0x9C92);
  background.drawString(data.halfHourFee.c_str(), 302, 52, GFXFF);

  // Print Difficulty
  background.setFreeFont(FSS9);
  background.setTextDatum(TR_DATUM);
  background.setTextColor(0x9C92);
  background.drawString(data.netwrokDifficulty.c_str(), 302, 88, GFXFF);

  // Print Global Hashrate
  render.setFontSize(17);
  render.rdrawString(data.globalHashRate.c_str(), 274, 145, TFT_BLACK);

  // Print BlockHeight
  render.setFontSize(28);
  render.rdrawString(data.blockHeight.c_str(), 140, 104, 0xDEDB);

  // Draw percentage rectangle
  int x2 = 2 + (138 * data.progressPercent / 100);
  background.fillRect(2, 149, x2, 168, 0xDEDB);

  // Print Remaining BLocks
  background.setTextFont(FONT2);
  background.setTextSize(1);
  background.setTextDatum(MC_DATUM);
  background.setTextColor(TFT_BLACK);
  background.drawString(data.remainingBlocks.c_str(), 72, 159, FONT2);
#endif

  // Push prepared background to screen
  background.pushSprite(0, 0);
}


void tDisplay_BTCprice(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

#ifdef GUARDIAN
  background.fillSprite(0x1082);
#ifdef NERD_NOS
  background.pushImage(0, 0, guardianMaxPriceWidth, guardianMaxPriceHeight, guardianMaxPriceScreen);
#else
  background.pushImage(0, 0, guardianPriceWidth, guardianPriceHeight, guardianPriceScreen);
#endif
  String price = guardianGrouped(data.btcPrice);
  OpenFontRender &pen = guardianFont(true);
  const int priceCenter = 160;
  const int priceY = 60;
  pen.setFontSize(40);
  if (!price.length())
    pen.cdrawString("--", priceCenter, priceY, gWhite, gBg);
  else
  {
    uint32_t numW = pen.getTextWidth("%s", price.c_str());
    uint32_t numH = pen.getTextHeight("%s", price.c_str());
    pen.cdrawString(price.c_str(), priceCenter, priceY, gWhite, gBg);
    pen.setFontSize(20);
    uint32_t usdH = pen.getTextHeight("USD");
    int usdY = priceY + (int)numH - (int)usdH;
    pen.drawString("USD", priceCenter + (int)numW / 2 + 6, usdY, gCyan, gBg);
  }
  guardianBottomBars(data.currentHashRate.c_str(), data.blockHeight.c_str());
#else
  data.currentDate ="01/12/2023";

  //if(data.currentDate.indexOf("12/2023")>) { tDisplay_ChristmasContent(data); return; }

  // Print background screen
  background.pushImage(0, 0, priceScreenWidth, priceScreenHeight, priceScreen);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());

  // Hashrate
  render.setFontSize(25);
  render.setCursor(19, 122);
  render.setFontColor(TFT_BLACK);
  render.rdrawString(data.currentHashRate.c_str(), 94, 129, TFT_BLACK);

  // Print BlockHeight
  render.setFontSize(18);
  render.rdrawString(data.blockHeight.c_str(), 254, 138, TFT_WHITE);

  // Print Hour
  
  background.setFreeFont(FSSB9);
  background.setTextSize(1);
  background.setTextDatum(TL_DATUM);
  background.setTextColor(TFT_BLACK);
  background.drawString(data.currentTime.c_str(), 222, 3, GFXFF);

  // Print BTC Price 
  background.setFreeFont(FF24);
  background.setTextDatum(TR_DATUM);
  background.setTextSize(1);
  background.setTextColor(0xDEDB, TFT_BLACK);
  background.drawString(data.btcPrice.c_str(), 300, 58, GFXFF);
#endif

  // Push prepared background to screen
  background.pushSprite(0, 0);
}

void tDisplay_LoadingScreen(void)
{
#ifdef GUARDIAN
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, guardianInitWidth, guardianInitHeight, guardianInitScreen);
#else
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, initWidth, initHeight, initScreen);
  tft.setTextColor(TFT_BLACK);
  tft.drawString(CURRENT_VERSION, 24, 147, FONT2);
#endif
}

void tDisplay_SetupScreen(void)
{
#ifdef GUARDIAN
  tft.pushImage(0, 0, guardianSetupWidth, guardianSetupHeight, guardianSetupScreen);
#else
  tft.pushImage(0, 0, setupModeWidth, setupModeHeight, setupModeScreen);
#endif
}

void tDisplay_AnimateCurrentScreen(unsigned long frame)
{
}

void tDisplay_DoLedStuff(unsigned long frame)
{
}

CyclicScreenFunction tDisplayCyclicScreens[] = {tDisplay_MinerScreen, tDisplay_ClockScreen, tDisplay_GlobalHashScreen, tDisplay_BTCprice};

DisplayDriver tDisplayDriver = {
    tDisplay_Init,
    tDisplay_AlternateScreenState,
    tDisplay_AlternateRotation,
    tDisplay_LoadingScreen,
    tDisplay_SetupScreen,
    tDisplayCyclicScreens,
    tDisplay_AnimateCurrentScreen,
    tDisplay_DoLedStuff,
    SCREENS_ARRAY_SIZE(tDisplayCyclicScreens),
    0,
    WIDTH,
    HEIGHT};
#endif
