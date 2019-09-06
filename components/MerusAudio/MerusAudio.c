//
// MA120x0P ESP32 Driver 
//
// Merus Audio - September 2018
// Written by Joergen Kragh Jakobsen, jkj@myrun.dk
//
// Register interface thrugh I2C for MA12070P and MA12040P 
//   Support a single amplifier/i2c address
//   
// 

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "MerusAudio.h"

#include "ma120x0.h"

#define MA_NENABLE_IO  CONFIG_MA120X0_NENABLE_PIN  
#define MA_NMUTE_IO    CONFIG_MA120X0_NMUTE_PIN 

static const char* I2C_TAG = "i2c";
#define I2C_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(I2C_TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }


#define I2C_MASTER_SCL_IO CONFIG_MA120X0_SCL_PIN  //4   /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_MA120X0_SDA_PIN  //0
    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0
 /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define MA120X0_ADDR  CONFIG_MA120X0_I2C_ADDR  /*!< slave address for MA120X0 amplifier */

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */


void setup_ma120x0()
{  // Setup control pins nEnable and nMute
   gpio_config_t io_conf;

   io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
   io_conf.mode = GPIO_MODE_OUTPUT;
   io_conf.pin_bit_mask = (1ULL<<MA_NENABLE_IO | 1ULL<<MA_NMUTE_IO);
   io_conf.pull_down_en = 0;
   io_conf.pull_up_en = 0;

   gpio_config(&io_conf);
    
   gpio_set_level(MA_NMUTE_IO, 0);
   gpio_set_level(MA_NENABLE_IO, 1);

   i2c_master_init();
    
   gpio_set_level(MA_NENABLE_IO, 0);
	  
   uint8_t res = ma_read_byte(MA_hw_version__a);
   printf("Hardware version: 0x%02x\n",res);

   ma_write_byte(MA_i2s_format__a,9);          // Set i2s left justified, set audio_proc_enable
   ma_write_byte(MA_vol_db_master__a,0x20);    // Set vol_db_master 

   res = ma_read_byte(MA_error__a); 
   printf("Errors : 0x%02x\n",res);

   res = get_MA_audio_in_mode_mon();
   printf("Audio in mode : 0x%02x\n",res);
     
   printf("Clear errors\n");
   ma_write_byte(45,0x34);
   ma_write_byte(45,0x30);
   printf("MA120x0P init done\n");

   gpio_set_level(MA_NMUTE_IO, 1);
   printf("Unmute\n");
}


void i2c_master_init()
{  int i2c_master_port = I2C_MASTER_NUM;
   i2c_config_t conf;
   conf.mode = I2C_MODE_MASTER;
   conf.sda_io_num = I2C_MASTER_SDA_IO;
   conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
   conf.scl_io_num = I2C_MASTER_SCL_IO;
   conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
   conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
   esp_err_t res = i2c_param_config(i2c_master_port, &conf);
   printf("Driver param setup : %d\n",res);
   res = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
   printf("Driver installed   : %d\n",res);
}

esp_err_t ma_write(uint8_t address, uint8_t *wbuf, uint8_t n)
{
  bool ack = ACK_VAL;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, MA120X0_ADDR<<1 | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, address, ACK_VAL);

  for (int i=0 ; i<n ; i++)
  { if (i==n-1) ack = NACK_VAL;
    i2c_master_write_byte(cmd, wbuf[i], ack);
  }
  i2c_master_stop(cmd);
  int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) { return ret; }
  return ESP_OK;
}

esp_err_t ma_write_byte(uint8_t address, uint8_t value)
{ esp_err_t ret=0;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MA120X0_ADDR<<1) | WRITE_BIT , ACK_CHECK_EN);
  i2c_master_write_byte(cmd, address, ACK_VAL);
  i2c_master_write_byte(cmd, value, ACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
     printf("ESP_I2C_WRITE ERROR : %d\n",ret);
	 return ret;
  }
  return ESP_OK;
}

esp_err_t ma_read(uint8_t address, uint8_t *rbuf, uint8_t n)
{ esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == NULL ) { printf("ERROR handle null\n"); }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MA120X0_ADDR<<1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, address, ACK_VAL);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MA120X0_ADDR<<1) | READ_BIT, ACK_CHECK_EN);

  i2c_master_read(cmd, rbuf, n-1 ,ACK_VAL);
 // for (uint8_t i = 0;i<n;i++)
 // { i2c_master_read_byte(cmd, rbuf++, ACK_VAL); }
  i2c_master_read_byte(cmd, rbuf + n-1 , NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
      printf("i2c Error read - readback\n");
	  return ESP_FAIL;
  }
  return ret;
}


uint8_t ma_read_byte(uint8_t address)
{
  uint8_t value = 0;
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);								// Send i2c start on bus
  i2c_master_write_byte(cmd, (MA120X0_ADDR<<1) | WRITE_BIT, ACK_CHECK_EN );
  i2c_master_write_byte(cmd, address, ACK_VAL);         // Send address to start read from
  i2c_master_start(cmd);							    // Repeated start
  i2c_master_write_byte(cmd, (MA120X0_ADDR<<1) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &value, NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
      printf("i2c Error read - readback\n");
	  return ESP_FAIL;
  }

  return value;
}
