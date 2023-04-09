#include <stdio.h>
#include <string>
#include <time.h>
#include <vector>
#include <ctype.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "sd_card.h"
#include "ff.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

enum
{
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

struct Button
{
  uint8_t buttonPin;
  uint8_t keyCode[6] = { 0 };
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void hid_task(void);
void init_buttons(void);
void sd_card_init(void);
void initialize_sd_card(void);
static void cdc_task(void);

void split_data(void);
int string_to_hid(std::string keyData);

FRESULT fr;
FATFS fs;
FIL fil;
int ret;
char buf[100] = { 0 };
char filename[] = "data.txt";
bool init_once = true;

std::vector<int> buttonPins{26, 27};
std::vector<Button> buttonGroup;
std::vector<std::vector<std::string>> splitVectorData;

int main()
{
  stdio_init_all();
  stdio_usb_init();
  board_init();
  tusb_init();
  //tud_init(BOARD_TUD_RHPORT);
  initialize_sd_card();
  split_data();
  init_buttons();

  bool bootMode = gpio_get(buttonGroup[0].buttonPin);

  while (1)
  {
    tud_task();
    cdc_task();
    hid_task();

    if(gpio_get(buttonGroup[0].buttonPin))
    {
      if(init_once)
      {
        init_once = false;
      }
    }
    else
    {

    }    
  }
}

//--------------------------------------------------------------------+
// SD Card 
//--------------------------------------------------------------------+
void initialize_sd_card(void)
{
  // Initialize SD card
  if (!sd_init_driver()) {
    printf("ERROR: Could not initialize SD card\r\n");
    while (true);
  }

  // Mount drive
  fr = f_mount(&fs, "0:", 1);
  if (fr != FR_OK) {
    printf("ERROR: Could not mount filesystem (%d)\r\n", fr);
    while (true);
  }

  // Open file for reading
  fr = f_open(&fil, filename, FA_READ);
  if (fr != FR_OK) {
    printf("ERROR: Could not open file (%d)\r\n", fr);
    while (true);
  }

  // Print every line in file over serial
  printf("Reading from file '%s':\r\n", filename);
  printf("---\r\n");
  while (f_gets(buf, sizeof(buf), &fil)) {
    printf(buf);
  }
  printf("\r\n---\r\n");

  // Close file
  fr = f_close(&fil);
  if (fr != FR_OK) {
    printf("ERROR: Could not close file (%d)\r\n", fr);
    while (true);
  }
}

//--------------------------------------------------------------------+
// Serial USB CDC
//--------------------------------------------------------------------+
// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  uint8_t const case_diff = 'a' - 'A';

  for(uint32_t i=0; i<count; i++)
  {
    if (itf == 0)
    {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += case_diff;
    }
    else
    {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= case_diff;
    }

    tud_cdc_n_write_char(itf, buf[i]);
  }
  tud_cdc_n_write_flush(itf);
}

static void cdc_task(void)
{
  uint8_t itf;

  for (itf = 0; itf < CFG_TUD_CDC; itf++)
  {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_n_connected(itf) )
    {
      if ( tud_cdc_n_available(itf) )
      {
        uint8_t buf[64];

        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

        // echo back to both serial ports
        echo_serial_port(0, buf, count);
        echo_serial_port(1, buf, count);
      }
    }
  }
}

//--------------------------------------------------------------------+
// Func
//--------------------------------------------------------------------+
void init_buttons(void)
{
  for (int i = 0; i < splitVectorData.size(); i++)
  {
    Button b;
    b.buttonPin = buttonPins[i];
    for (int j = 0; j < splitVectorData[i].size(); j++)
    {
      b.keyCode[j] = string_to_hid(splitVectorData[i][j]);
      printf(splitVectorData[i][j].c_str());
      printf("\r\n");
    }
    buttonGroup.push_back(b);
    gpio_init(b.buttonPin);
    gpio_set_dir(b.buttonPin, GPIO_IN);
    printf("---\r\n");
  }
}

void split_data(void)
{
  std::vector<std::string> splitData;
  std::string splitStr = "";

  for (int i = 0; i < sizeof(buf); i++)
  {
    if(buf[i] != 0)
    {
      if(buf[i] == '+')
      {
        splitData.push_back(splitStr);
        splitStr = "";
        continue;
      }
      else if(buf[i] == ' ')
      {
        splitData.push_back(splitStr);
        splitVectorData.push_back(splitData);
        splitStr = "";
        splitData.clear();
        continue;
      }
      splitStr.push_back(buf[i]);
    }
  }
  splitData.push_back(splitStr);
  splitVectorData.push_back(splitData);
}

int string_to_hid(std::string keyData)
{
  if(keyData == "CTRL")
  {
    return HID_KEY_CONTROL_LEFT;
  }
  else if(keyData == "a")
  {
    return HID_KEY_A;
  }
  else if(keyData == "b")
  {
    return HID_KEY_B;
  }
  else if(keyData == "c")
  {
    return HID_KEY_C;
  }
  return HID_KEY_NONE;
}
//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
static void send_hid_report(uint8_t report_id, uint32_t btn_press, Button btn)
{
  if (!tud_hid_ready())
    return;

  switch (report_id)
  {
  case HID_INSTANCE_KEYBOARD:
  {
    static bool has_keyboard_key = false;

    if (btn_press)
    {
      //printf("tud hid report\r\n");
      tud_hid_keyboard_report(HID_INSTANCE_KEYBOARD, 0, btn.keyCode);
      has_keyboard_key = true;
    }
    else
    {
      if (has_keyboard_key)
        tud_hid_keyboard_report(HID_INSTANCE_KEYBOARD, 0, NULL);
      has_keyboard_key = false;
    }
  }
  break;
  default:
    break;
  }
}

void hid_task(void)
{
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if (board_millis() - start_ms < interval_ms)
    return;
  start_ms += interval_ms;

  for (int i = 0; i < buttonGroup.size(); i++)
  {
    uint32_t const btn = gpio_get(buttonGroup[i].buttonPin);
    if (tud_suspended() && btn)
    {
      //printf("tud wakeup\r\n");
      tud_remote_wakeup();
    }
    else
    {
      //printf("send hid\r\n");
      send_hid_report(HID_INSTANCE_KEYBOARD, btn, buttonGroup[i]);
    }
  }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
  (void)instance;
  (void)len;

  uint8_t next_report_id = report[0] + 1;
  Button b;
  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read(), b);
  }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  (void)instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == HID_INSTANCE_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if (bufsize < 1)
        return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }
      else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}