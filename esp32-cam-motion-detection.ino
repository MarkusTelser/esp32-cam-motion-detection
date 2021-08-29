#include <WiFi.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"
#include "ESP32_MailClient.h"
#include "time.h"

// camera model settings
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#define WIDTH 320
#define HEIGHT 240
#define DOWN_SCALE_RATIO 0.10
#define PIXEL_DIFF_THRESHOLD 0.2
#define IMAGE_DIFF_THRESHOLD 0.1
#define REBOUND_IMAGE_COUNT 0
//#define PIXFORMAT PIXFORMAT_GRAYSCALE 
#define PIXFORMAT PIXFORMAT_RGB565

// mail settings
#define SENDVIAMAIL false
#define emailSenderAccount    "test@mail.com"
#define emailSenderPassword   "password123"
#define smtpServer            "smtp.mail.com"
#define smtpServerPort        420
#define emailRecipient        "test@mail.com"

#define LEDPIN 33
#define EEPROM_SIZE 20

// wifi settings
const char* ssid = "wifi name";
const char* password = "password123";

// ntp server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = "0";
const int   daylightOffset_sec = "0";

int pictureNumber = 0;
int currentReboundCount = 0;
const int NWIDTH = (WIDTH * DOWN_SCALE_RATIO);
const int NHEIGHT = (HEIGHT * DOWN_SCALE_RATIO);
uint16_t old_frame[NHEIGHT][NWIDTH] = { 256 };
uint16_t current_frame[NHEIGHT][NWIDTH] = { 0 };


//TODO let lights blink after successfull start up

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  //connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nIP Address: http://%s\n", WiFi.localIP().toString().c_str());
  
  // mount SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  
  // camera settings
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT; 

  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // mount SD-card
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    ESP.restart();
  }

  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    ESP.restart();
  }
  Serial.println("SD Card mounted");

  // initialize time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  // initialize LEDPIN as an output
  pinMode(LEDPIN, OUTPUT);
  blinkLight();
}


void loop() {
  int64_t fr_start = esp_timer_get_time();
  
  camera_fb_t * fb = takePhoto();
  if(detectMotion()){
    savePhoto(fb);
    if(SENDVIAMAIL)
      sendPhoto();
  }
  esp_camera_fb_return(fb);
  updateOldFrame();
  
  Serial.printf("%dkb free heap (min. %dkb)\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
  int64_t fr_end = esp_timer_get_time();
  uint32_t milli = (fr_end - fr_start) / 1000;
  Serial.printf("Took about %ums (%.2fFPS/%.1fFPM).\n", milli, 1000.0/milli, 60000.0/milli);  
}


camera_fb_t * takePhoto( void ) {
  Serial.println("Taking a photo...");
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return NULL;
  }
  
  // down-sample image in blocks
  if(PIXFORMAT == PIXFORMAT_GRAYSCALE){
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
      const uint8_t block_x = floor((i % WIDTH) * DOWN_SCALE_RATIO);
      const uint8_t block_y = floor(floor(i / WIDTH) * DOWN_SCALE_RATIO);
      const uint8_t pixel = fb->buf[i];

      // average pixels in block (accumulate)
      current_frame[block_y][block_x] += pixel;
    }
  }else if(PIXFORMAT == PIXFORMAT_RGB565){
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
      const uint8_t block_x = floor((i % WIDTH) * DOWN_SCALE_RATIO);
      const uint8_t block_y = floor(floor(i / WIDTH) * DOWN_SCALE_RATIO);
      const uint8_t pixel = (uint8_t)((fb->buf[i*3] + fb->buf[i*3+1] + fb->buf[i*3+2])/3);
      
      // average pixels in block (accumulate)
      current_frame[block_y][block_x] += pixel;
    }
  }
  
  // average pixels in block (rescale)
  for (int y = 0; y < NHEIGHT; y++)
      for (int x = 0; x < NWIDTH; x++)
          current_frame[y][x] /= (100 * DOWN_SCALE_RATIO) * (100 * DOWN_SCALE_RATIO);
          
  return fb;
}


bool detectMotion( void ){
  if(old_frame[0][0] == 256){
    Serial.println("No previous frame!");
    return false;
  }
  
  uint16_t changes = 0;
  const uint16_t blocks = NWIDTH * NHEIGHT;
  
  for (int y = 0; y < NHEIGHT; y++) {
      for (int x = 0; x < NWIDTH; x++) {
          float current = current_frame[y][x];
          float prev = old_frame[y][x];
          float delta = abs(current - prev) / prev;

          if (delta >= PIXEL_DIFF_THRESHOLD)
            changes += 1;
      }
  }

  Serial.printf("%d out of %d pixels changed (%d%%)\n", changes, blocks, (int)((1.0*changes/blocks)*100));

  // add rebound photo capture after event
  bool detected = (1.0 * changes / blocks) > IMAGE_DIFF_THRESHOLD;
  if(detected)
    currentReboundCount = REBOUND_IMAGE_COUNT;
  else if(currentReboundCount > 0){
    currentReboundCount -= 1;
    Serial.printf("storing %d out of %d rebound frames\n", REBOUND_IMAGE_COUNT - currentReboundCount, REBOUND_IMAGE_COUNT);
    return true;
  }
    
  return detected;
}

  
void savePhoto( camera_fb_t * fb ){
  if(fb == NULL){
    Serial.println("No photo provided to be saved!");
    esp_camera_fb_return(fb);
    return;
  }
  // initialize EEPROM with predefined size
  EEPROM.begin(1);
  pictureNumber = EEPROM.read(0) + 1;

  // Path where new picture will be saved in SD Card
  String path = "/picture" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC;

  uint8_t quality = 30;
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    uint8_t *jpeg;
    size_t len;
    if (fmt2jpg(fb->buf, WIDTH * HEIGHT, WIDTH, HEIGHT, PIXFORMAT, quality, &jpeg, &len)) {
      file.write(jpeg, len);
      free(jpeg);
    }
    Serial.printf("Saved file to path: %s\n", path.c_str());
  }
  file.close();
  EEPROM.write(0, pictureNumber);
  EEPROM.commit();

  // save to SPIFFS, if sended via mail
  if(SENDVIAMAIL){
    File file = SPIFFS.open("/photo.jpg", FILE_WRITE);
    if (!file)
      Serial.println("Failed to open file in writing mode");
    else{
      uint8_t *jpeg;
      size_t len;
      if (fmt2jpg(fb->buf, WIDTH * HEIGHT, WIDTH, HEIGHT, PIXFORMAT, quality, &jpeg, &len)) {
        file.write(jpeg, len);
        free(jpeg);
      }
      Serial.println("Saved file to SPIFFS");
    }
    file.close();
  }
}


void sendPhoto( void ) {
  Serial.println("Sending email...");

  // get time to generate subject, message
  char emailSubject[50];
  char emailMessage[100];
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(emailSubject, 50, "ESP-32 Photo %A %H:%M", &timeinfo);  
  strftime(emailMessage, 100, "<h2>Photo captured with ESP32-CAM at %H:%M:%S on %A in %B, %Y (%d.%m.%y)</h2>", &timeinfo);
  
  // set all the data
  SMTPData smtpData;
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  smtpData.setSender("ESP32-CAM", emailSenderAccount);
  smtpData.setPriority("High");  
  smtpData.setSubject(emailSubject);
  smtpData.setMessage(emailMessage, true);
  smtpData.addRecipient(emailRecipient);
  Serial.println(pictureNumber);
  smtpData.addAttachFile("/photo.jpg", "image/jpg");
  smtpData.setFileStorageType(MailClientStorageType::SPIFFS);
  smtpData.setSendCallback(sendCallback);
  
  // start sending mail
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

  smtpData.empty();
}


// callback function to get the mail sending status
void sendCallback(SendStatus msg) {
  Serial.println(msg.info());
}


void printLocalTime(){
  struct tm timeinfo;
  Serial.print("obtaining local time");
  while(!getLocalTime(&timeinfo)){
    Serial.print(".");
    delay(500);
  }
  Serial.println(&timeinfo, "\n%A, %B %d %Y %H:%M:%S");
}


void updateOldFrame() {
    for (int y = 0; y < NHEIGHT; y++) {
        for (int x = 0; x < NWIDTH; x++) {
            old_frame[y][x] = current_frame[y][x];
        }
    }
}


void blinkLight(){
  for(int i = 0; i < 3; i++){
    digitalWrite(LEDPIN, LOW);
    delay(1000);
    digitalWrite(LEDPIN, HIGH);
    delay(1000);
  }
}
