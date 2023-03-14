#include <stdio.h>
#include <string>
#include <time.h>
#include <vector>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "sd_card.h"
#include "ff.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

void sd_card_func();

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

FRESULT fr;
FATFS fs;
FIL fil;
int ret;
char buf[100];
char filename[] = "test02.txt";

std::vector<Button> buttonGroup;

int main()
{
  board_init();
  tusb_init();

  stdio_init_all();
  stdio_usb_init();

  init_buttons();

  while (1)
  {
    tud_task();
    hid_task();
  }
}

//--------------------------------------------------------------------+
// Func
//--------------------------------------------------------------------+
void init_buttons(void)
{
  Button b1;
  b1.buttonPin = 26;
  b1.keyCode[0] = HID_KEY_A;

  Button b2;
  b2.buttonPin = 27;
  b2.keyCode[0] = HID_KEY_CONTROL_LEFT;
  b2.keyCode[1] = HID_KEY_ALT_LEFT;
  b2.keyCode[2] = HID_KEY_DELETE;

  buttonGroup.push_back(b1);
  buttonGroup.push_back(b2);

  gpio_init(b1.buttonPin);
  gpio_init(b2.buttonPin);
  gpio_set_dir(b1.buttonPin, GPIO_IN);
  gpio_set_dir(b2.buttonPin, GPIO_IN);
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
  case REPORT_ID_KEYBOARD:
  {
    static bool has_keyboard_key = false;

    if (btn_press)
    {
      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, btn.keyCode);
      has_keyboard_key = true;
    }
    else
    {
      if (has_keyboard_key)
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
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
      tud_remote_wakeup();
    }
    else
    {
      send_hid_report(REPORT_ID_KEYBOARD, btn, buttonGroup[i]);
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
    if (report_id == REPORT_ID_KEYBOARD)
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

void sd_card_func()
{
  // Initialize SD card
  if (!sd_init_driver())
  {
    printf("ERROR: Could not initialize SD card\r\n");
    while (true)
      ;
  }

  // Mount drive
  fr = f_mount(&fs, "0:", 1);
  if (fr != FR_OK)
  {
    printf("ERROR: Could not mount filesystem (%d)\r\n", fr);
    while (true)
      ;
  }

  // Open file for writing ()
  fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
  if (fr != FR_OK)
  {
    printf("ERROR: Could not open file (%d)\r\n", fr);
    while (true)
      ;
  }

  // Write something to file
  ret = f_printf(&fil, "This is another test\r\n");
  if (ret < 0)
  {
    printf("ERROR: Could not write to file (%d)\r\n", ret);
    f_close(&fil);
    while (true)
      ;
  }
  ret = f_printf(&fil, "of writing to an SD card.\r\n");
  if (ret < 0)
  {
    printf("ERROR: Could not write to file (%d)\r\n", ret);
    f_close(&fil);
    while (true)
      ;
  }

  // Close file
  fr = f_close(&fil);
  if (fr != FR_OK)
  {
    printf("ERROR: Could not close file (%d)\r\n", fr);
    while (true)
      ;
  }

  // Open file for reading
  fr = f_open(&fil, filename, FA_READ);
  if (fr != FR_OK)
  {
    printf("ERROR: Could not open file (%d)\r\n", fr);
    while (true)
      ;
  }

  // Print every line in file over serial
  printf("Reading from file '%s':\r\n", filename);
  printf("---\r\n");
  while (f_gets(buf, sizeof(buf), &fil))
  {
    printf(buf);
  }
  printf("\r\n---\r\n");

  // Close file
  fr = f_close(&fil);
  if (fr != FR_OK)
  {
    printf("ERROR: Could not close file (%d)\r\n", fr);
    while (true)
      ;
  }

  // Unmount drive
  f_unmount("0:");
}