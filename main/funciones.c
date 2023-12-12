
#include "funciones.h"

char state[100];

uint32_t BotonEncState =off;
uint32_t EstadoSistema =off;
uint32_t adc_val1;
double tv;
double tr;
double y;
double temp;


extern uint32_t espacio_total_personas=10;
uint32_t espacio_ahora_personas=0;



lv_disp_t * init_oled(void){
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST, I2C_MODE_MASTER, 0, 0, 0));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = EXAMPLE_I2C_HW_ADDR,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = EXAMPLE_LCD_CMD_BITS, // According to SSD1306 datasheet
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
        .dc_bit_offset = 0,                     // According to SH1107 datasheet
        .flags =
        {
            .disable_control_phase = 1,
        }
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
    };
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);
    /* Register done callback for IO */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp);

    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);

    ESP_LOGI(TAG, "Display LVGL Scroll Text");


    return(disp);
}

void init_gpios(void){
    /*ENTRADAS*/

    GpioModeInput(BTN1);
    GpioPullUpEnable(BTN1);

    GpioModeInput(BTN2);
    GpioPullUpEnable(BTN2);

    GpioModeInput(GPIO15);
    GpioPullUpEnable(GPIO15);



    /*SALIDAS*/
    GpioModeOutput(LED1);
    GpioModeOutput(LED2);
    GpioModeOutput(LED3);
    GpioModeOutput(LED4);


}
void init_adc(void){
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);

}

void actualizar_entradas(void){

    if(!GpioDigitalRead(BTN1) && BotonEncState==off){
        BotonEncState=on;
        EstadoSistema=on;
    }

    if(EstadoSistema==on){
        /*LEEMOS LA TEMPERATURA DEL SALON */
        adc_val1 = adc1_get_raw(ADC1_CHANNEL_0);

        /*SENSOR S_IN*/

        if(!GpioDigitalRead(S_IN)){
            while(!GpioDigitalRead(S_IN));
            if(espacio_ahora_personas<espacio_total_personas){
                espacio_ahora_personas++;    
            }
        }

        if(!GpioDigitalRead(GPIO15)){
            while(!GpioDigitalRead(GPIO15));
            if(espacio_ahora_personas>0){
                espacio_ahora_personas--;    
            }
        }




    }
    



}

void actualizar_salidas(void){
    static uint32_t bandera = 1;
    if(BotonEncState==on && bandera==1){
        GpioDigitalWrite(LED1,GPIO_HIGH);
        bandera=0;
    }

    if(BotonEncState==on){
        tv = ((3.3)*(adc_val1))/4095.0;
        tr = ((tv)*(10000.0))/(3.3-tv);
         y = log(tr/10000.0);
         y = (1.0/298.15)+(y*(1.0/4050.0));
        temp = 1.0/y;
        temp = temp -273.15;
    }

}

void imprimir_oled(lv_obj_t *label){

    static uint32_t bandera =1;
    
    if(BotonEncState==on && bandera==1){
    sprintf(state,"Sistema: ON");
    lv_label_set_text(label, state);
    bandera=0;
    }

    if(BotonEncState==on){
        sprintf(state,"Sistema: ON\nDoor: Closed\nTemp:%.2f°C",temp);
        lv_label_set_text(label, state);

    }

}
void imprimir_terminal(){
    static uint32_t bandera =1;
    
    if(BotonEncState==on && bandera==1){
    sprintf(state,"Sistema: ON");
    printf("%s", state);
    bandera=0;
    }

    if(BotonEncState==on){
        sprintf(state,"Sistema: ON  |  Door: Closed  |  Temp:%.2f°C  | Personas: %ld de %ld  |  \n",temp,espacio_ahora_personas,espacio_total_personas);
        printf("%s", state);

    }



}