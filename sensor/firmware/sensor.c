#include "sensor.h"

#include "app_timer.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_drv_adc.h"


#define NRF_LOG_MODULE_NAME "SENSOR"
#define NRF_LOG_LEVEL 3
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define BME280_SPI_BUFFER_LEN 32
#define BME280_SPI_CS_PIN 5
#define BME280_SPI_SCK_PIN 1
#define BME280_SPI_MOSI_PIN 3
#define BME280_SPI_MISO_PIN 2

#define BME280_RA_MEASURMENT_DATA 0xF7 // 8 bytes
#define BME280_RA_CTRL_MEAS 0xF4
#define BME280_RA_CTRL_HUM 0xF2
#define BME280_RA_CHIP_ID 0xD0 // must be 0x60
#define BME280_RA_CALIB00 0x88 //26bytes
#define BME280_RA_CALIB26 0xE1 //16bytes

#define SENSOR_MEASUREMENT_WAIT_TIME 100

#define MARGE_16BIT(H, L) ((((uint16_t)H) << 8) | ((uint16_t)L))
#define MARGE_20BIT(H, L, XL) ((((uint32_t)H) << 12) | (((uint32_t)L) << 4) | (((uint32_t)XL) >> 4))

// ADC config
// reference : internal 1.2V
// input : VDD / 3
static const nrf_drv_adc_channel_t adc_channel_config =
    {{{.resolution = NRF_ADC_CONFIG_RES_10BIT,
       .input = NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD,
       .reference = NRF_ADC_CONFIG_REF_VBG,
       .ain = NRF_ADC_CONFIG_INPUT_DISABLED}}};

static const nrf_drv_adc_config_t adc_config = NRF_DRV_ADC_DEFAULT_CONFIG;

// SPI config
static nrf_drv_spi_config_t spi_config =
    {
        .ss_pin = NRF_DRV_SPI_PIN_NOT_USED,
        .sck_pin = BME280_SPI_SCK_PIN,
        .mosi_pin = BME280_SPI_MOSI_PIN,
        .miso_pin = BME280_SPI_MISO_PIN,
        .irq_priority = SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
        .orc = 0xAA,
        .frequency = NRF_DRV_SPI_FREQ_1M,
        .mode = NRF_DRV_SPI_MODE_0,
        .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST};

static const nrf_drv_spi_t m_bme280_spi_master = NRF_DRV_SPI_INSTANCE(0);
static uint8_t m_bme280_spi_tx_buffer[BME280_SPI_BUFFER_LEN]; ///< SPI master TX buffer.
static uint8_t m_bme280_spi_rx_buffer[BME280_SPI_BUFFER_LEN]; ///< SPI master RX buffer.
static volatile bool m_bme280_spi_transfer_completed = false;

static sensor_data_handler_t m_sensor_data_handler = NULL;
static volatile bool m_bme280_receiving_measurement_data = false;
static SensorMeasurementData m_sensor_measurment_data;
static nrf_adc_value_t m_adc_result;

APP_TIMER_DEF(m_sensor_measurement_wait_timer_id);

// calibration parameters
// details are in datesheet of BME280
static struct
{
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint8_t dig_H1;
    int16_t dig_H2;
    uint8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
    int32_t t_fine;
} m_bme280_calib_data;


static void bme280_spi_master_event_handler(nrf_drv_spi_evt_t const *p_event);

static void bme280_spi_assert_cs()
{
    nrf_gpio_pin_clear(BME280_SPI_CS_PIN);
}

static void bme280_spi_deassert_cs()
{
    nrf_gpio_pin_set(BME280_SPI_CS_PIN);
}

static uint32_t bme280_spi_start_write_reg_byte(uint8_t reg_addr, uint8_t data)
{
    uint32_t err_code;

    m_bme280_spi_tx_buffer[0] = reg_addr & 0x7f;
    m_bme280_spi_tx_buffer[1] = data;

    m_bme280_spi_transfer_completed = false;

    err_code = nrf_drv_spi_init(&m_bme280_spi_master, &spi_config, bme280_spi_master_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    bme280_spi_assert_cs();
    err_code = nrf_drv_spi_transfer(&m_bme280_spi_master, m_bme280_spi_tx_buffer, 2, m_bme280_spi_rx_buffer, 2);

    return err_code;
}

static uint32_t bme280_spi_start_read_reg_bytes(uint8_t reg_addr, uint8_t len)
{
    uint32_t err_code;

    m_bme280_spi_tx_buffer[0] = reg_addr | 0x80;
    m_bme280_spi_tx_buffer[1] = 0x00;

    m_bme280_spi_transfer_completed = false;

    err_code = nrf_drv_spi_init(&m_bme280_spi_master, &spi_config, bme280_spi_master_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    bme280_spi_assert_cs();
    err_code = nrf_drv_spi_transfer(&m_bme280_spi_master, m_bme280_spi_tx_buffer, len + 1, m_bme280_spi_rx_buffer, len + 1);

    return err_code;
}

static void bme280_spi_wait_for_transfer_complete()
{
    while (!m_bme280_spi_transfer_completed)
        ;
    nrf_drv_spi_uninit(&m_bme280_spi_master);
}

// calibration code is cited from below url.
// https://github.com/BoschSensortec/BME280_driver

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of 5123 equals 51.23 DegC.
// t_fine carries fine temperature as global value
static int32_t bme280_compensate_temperature(uint32_t uncomp_data)
{
    int32_t var1, var2, temperature;
    const int32_t temperature_min = -4000;
    const int32_t temperature_max = 8500;

    var1 = (int32_t)(((int32_t)uncomp_data >> 3) - ((int32_t)m_bme280_calib_data.dig_T1 << 1));
    var1 = (var1 * ((int32_t)m_bme280_calib_data.dig_T2)) >> 11;
    var2 = (int32_t)(((int32_t)uncomp_data >> 4) - ((int32_t)m_bme280_calib_data.dig_T1));
    var2 = (((var2 * var2) >> 12) * ((int32_t)m_bme280_calib_data.dig_T3)) >> 14;
    m_bme280_calib_data.t_fine = var1 + var2;
    temperature = (m_bme280_calib_data.t_fine * 5 + 128) >> 8;

    if (temperature < temperature_min)
    {
        temperature = temperature_min;
    }
    else if (temperature > temperature_max)
    {
        temperature = temperature_max;
    }

    return temperature;
}

// Returns pressure in Pa as unsigned 32 bit integer. Output value of 96386 equals 96386 Pa = 963.86 hPa
static uint32_t bme280_compensate_pressure(uint32_t uncomp_data)
{
    int32_t var1, var2, var3, var4;
    uint32_t var5;
    uint32_t pressure;
    const uint32_t pressure_min = 30000;
    const uint32_t pressure_max = 110000;

    var1 = (((int32_t)m_bme280_calib_data.t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) / 11) * ((int32_t)m_bme280_calib_data.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)m_bme280_calib_data.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)m_bme280_calib_data.dig_P4) << 16);
    var3 = (m_bme280_calib_data.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3;
    var4 = (((int32_t)m_bme280_calib_data.dig_P2) * var1) >> 1;
    var1 = (var3 + var4) >> 18;
    var1 = (((32768 + var1)) * ((int32_t)m_bme280_calib_data.dig_P1)) >> 15;
    /* avoid exception caused by division by zero */
    if (var1)
    {
        var5 = (uint32_t)((uint32_t)1048576) - uncomp_data;
        pressure = ((uint32_t)(var5 - (uint32_t)(var2 >> 12))) * 3125;
        if (pressure < 0x80000000)
        {
            pressure = (pressure << 1) / ((uint32_t)var1);
        }
        else
        {
            pressure = (pressure / (uint32_t)var1) * 2;
        }

        var1 = (((int32_t)m_bme280_calib_data.dig_P9) * ((int32_t)(((pressure >> 3) * (pressure >> 3)) >> 13))) >> 12;
        var2 = (((int32_t)(pressure >> 2)) * ((int32_t)m_bme280_calib_data.dig_P8)) >> 13;
        pressure = (uint32_t)((int32_t)pressure + ((var1 + var2 + m_bme280_calib_data.dig_P7) >> 4));

        if (pressure < pressure_min)
        {
            pressure = pressure_min;
        }
        else if (pressure > pressure_max)
        {
            pressure = pressure_max;
        }
    }
    else
    {
        pressure = pressure_min;
    }

    return pressure;
}

// Returns humidity in %, resolution is 0.001 %. Output value of 51232 equals 51.232 %.
static uint32_t bme280_compensate_humidity(uint32_t uncomp_data)
{
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    uint32_t humidity;
    const uint32_t humidity_max = 102400;

    var1 = m_bme280_calib_data.t_fine - ((int32_t)76800);
    var2 = (int32_t)(uncomp_data << 14);
    var3 = (int32_t)(((int32_t)m_bme280_calib_data.dig_H4) << 20);
    var4 = ((int32_t)m_bme280_calib_data.dig_H5) * var1;
    var5 = (((var2 - var3) - var4) + (int32_t)16384) >> 15;
    var2 = (var1 * ((int32_t)m_bme280_calib_data.dig_H6)) >> 10;
    var3 = (var1 * ((int32_t)m_bme280_calib_data.dig_H3)) >> 11;
    var4 = ((var2 * (var3 + (int32_t)32768)) >> 10) + (int32_t)2097152;
    var2 = ((var4 * ((int32_t)m_bme280_calib_data.dig_H2)) + 8192) >> 14;
    var3 = var5 * var2;
    var4 = ((var3 >> 15) * (var3 >> 15)) >> 7;
    var5 = var3 - ((var4 * ((int32_t)m_bme280_calib_data.dig_H1)) >> 4);
    var5 = (var5 < 0 ? 0 : var5);
    var5 = (var5 > 419430400 ? 419430400 : var5);
    humidity = (uint32_t)(var5 >> 12);

    if (humidity > humidity_max)
    {
        humidity = humidity_max;
    }

    return humidity;
}

static void parse_sensor_data()
{
    uint32_t temperature_uncomp_data = MARGE_20BIT(m_bme280_spi_rx_buffer[4], m_bme280_spi_rx_buffer[5], m_bme280_spi_rx_buffer[6]);
    uint32_t pressure_uncomp_data = MARGE_20BIT(m_bme280_spi_rx_buffer[1], m_bme280_spi_rx_buffer[2], m_bme280_spi_rx_buffer[3]);
    uint32_t humidity_uncomp_data = MARGE_16BIT(m_bme280_spi_rx_buffer[7], m_bme280_spi_rx_buffer[8]);

    // temperature in DegC, resolution is 0.01 DegC
    int32_t temperature_data = bme280_compensate_temperature(temperature_uncomp_data);
    // pressure in Pa
    uint32_t pressure_data = bme280_compensate_pressure(pressure_uncomp_data);
    // humidity in %, resolution is 0.001 %
    uint32_t humidity_data = bme280_compensate_humidity(humidity_uncomp_data);

    // temperature in ℃ multiplied by 10
    m_sensor_measurment_data.temperature = (int16_t)(temperature_data / 10);
    // air pressure in hPa
    m_sensor_measurment_data.pressure = (uint16_t)(pressure_data / 100);
    // humidity in % multiplied by 10
    m_sensor_measurment_data.humidity = (uint16_t)(humidity_data / 100);
    // battery voltage in mV
    m_sensor_measurment_data.battery = (uint16_t)((uint32_t)m_adc_result * 3600 / 1024);
}

void bme280_spi_master_event_handler(nrf_drv_spi_evt_t const *p_event)
{
    switch (p_event->type)
    {
    case NRF_DRV_SPI_EVENT_DONE:
        if (m_bme280_receiving_measurement_data)
        {
            parse_sensor_data();

            if (m_sensor_data_handler)
            {
                m_sensor_data_handler(&m_sensor_measurment_data);
            }
            m_bme280_receiving_measurement_data = false;

            nrf_drv_adc_uninit();
        }

        m_bme280_spi_transfer_completed = true;
        bme280_spi_deassert_cs();

        nrf_drv_spi_uninit(&m_bme280_spi_master);
        break;

    default:
        // No implementation needed.
        break;
    }
}

static uint32_t bme280_check_chip_id()
{
    uint32_t err_code = bme280_spi_start_read_reg_bytes(BME280_RA_CHIP_ID, 1);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    bme280_spi_wait_for_transfer_complete();

    if (m_bme280_spi_rx_buffer[1] != 0x60)
    {
        return 1;
    }

    return NRF_SUCCESS;
}

static uint32_t bme280_read_calibration_data()
{
    uint32_t err_code;
    err_code = bme280_spi_start_read_reg_bytes(BME280_RA_CALIB00, 26);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    bme280_spi_wait_for_transfer_complete();

    m_bme280_calib_data.dig_T1 = MARGE_16BIT(m_bme280_spi_rx_buffer[2], m_bme280_spi_rx_buffer[1]);
    m_bme280_calib_data.dig_T2 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[4], m_bme280_spi_rx_buffer[3]);
    m_bme280_calib_data.dig_T3 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[6], m_bme280_spi_rx_buffer[5]);

    m_bme280_calib_data.dig_P1 = MARGE_16BIT(m_bme280_spi_rx_buffer[8], m_bme280_spi_rx_buffer[7]);
    m_bme280_calib_data.dig_P2 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[10], m_bme280_spi_rx_buffer[9]);
    m_bme280_calib_data.dig_P3 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[12], m_bme280_spi_rx_buffer[11]);
    m_bme280_calib_data.dig_P4 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[14], m_bme280_spi_rx_buffer[13]);
    m_bme280_calib_data.dig_P5 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[16], m_bme280_spi_rx_buffer[15]);
    m_bme280_calib_data.dig_P6 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[18], m_bme280_spi_rx_buffer[17]);
    m_bme280_calib_data.dig_P7 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[20], m_bme280_spi_rx_buffer[19]);
    m_bme280_calib_data.dig_P8 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[22], m_bme280_spi_rx_buffer[21]);
    m_bme280_calib_data.dig_P9 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[24], m_bme280_spi_rx_buffer[23]);

    m_bme280_calib_data.dig_H1 = m_bme280_spi_rx_buffer[26];

    err_code = bme280_spi_start_read_reg_bytes(BME280_RA_CALIB26, 6);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    bme280_spi_wait_for_transfer_complete();

    m_bme280_calib_data.dig_H2 = (int16_t)MARGE_16BIT(m_bme280_spi_rx_buffer[2], m_bme280_spi_rx_buffer[1]);
    m_bme280_calib_data.dig_H3 = m_bme280_spi_rx_buffer[3];
    m_bme280_calib_data.dig_H4 = (int16_t)((((uint16_t)m_bme280_spi_rx_buffer[4]) << 4) | (m_bme280_spi_rx_buffer[5] & 0x0f));
    m_bme280_calib_data.dig_H5 = (int16_t)((((uint16_t)m_bme280_spi_rx_buffer[6]) << 4) | (m_bme280_spi_rx_buffer[5] >> 4));

    return NRF_SUCCESS;
}

static uint32_t bme280_read_measurement_data()
{
    uint32_t err_code;
    err_code = bme280_spi_start_read_reg_bytes(BME280_RA_MEASURMENT_DATA, 8);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}

static uint32_t bme280_config_measurement()
{
    uint32_t err_code;

    // [2:0] osrs_h=1 : humidity oversample x1
    err_code = bme280_spi_start_write_reg_byte(BME280_RA_CTRL_HUM, 1);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    bme280_spi_wait_for_transfer_complete();

    // [1:0] mode=0 : enter sleep mode
    // [4:2] osrs_p=1 : pressure oversample x1
    // [7:5] osrs_t=1 : temperature oversample x1
    err_code = bme280_spi_start_write_reg_byte(BME280_RA_CTRL_MEAS, 0x24);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    bme280_spi_wait_for_transfer_complete();

    return NRF_SUCCESS;
}

static void sensor_mesurement_wait_timer_handler()
{
    uint32_t err_code;

    m_bme280_receiving_measurement_data = true;

    err_code = bme280_read_measurement_data();
    APP_ERROR_CHECK(err_code);
}

uint32_t sensor_init(sensor_data_handler_t sensor_data_handler)
{
    uint32_t err_code = NRF_SUCCESS;

    m_sensor_data_handler = sensor_data_handler;

    nrf_gpio_cfg_output(BME280_SPI_CS_PIN);
    nrf_gpio_pin_set(BME280_SPI_CS_PIN);

    // chech chip ID
    err_code = bme280_check_chip_id();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = bme280_read_calibration_data();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = bme280_config_measurement();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_create(&m_sensor_measurement_wait_timer_id, APP_TIMER_MODE_SINGLE_SHOT, sensor_mesurement_wait_timer_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}

static void adc_evt_handler(nrf_drv_adc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_ADC_EVT_DONE)
    {
        NRF_LOG_DEBUG("ADC %u %u\n", p_event->data.done.size, p_event->data.done.p_buffer[0]);
    }
}

uint32_t sensor_start_measuring()
{
    uint32_t err_code;

    err_code = nrf_drv_adc_init(&adc_config, adc_evt_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_adc_channel_enable((nrf_drv_adc_channel_t *const)&adc_channel_config);

    err_code = nrf_drv_adc_buffer_convert(&m_adc_result, 1);
    APP_ERROR_CHECK(err_code);

    // start adc sample
    nrf_drv_adc_sample();

    // [1:0] mode=01 : start force mode
    // [4:2] osrs_p=1 : pressure oversample x1
    // [7:5] osrs_t=1 : temperature oversample x1
    err_code = bme280_spi_start_write_reg_byte(BME280_RA_CTRL_MEAS, 0x25);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_start(m_sensor_measurement_wait_timer_id, APP_TIMER_TICKS(SENSOR_MEASUREMENT_WAIT_TIME, 0), NULL);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}