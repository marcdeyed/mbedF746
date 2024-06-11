#include <mbed.h>
#include <threadLvgl.h>
#include "lvgl.h"
#include <stdio.h>

// Initialisation des broches et des objets matériels
AnalogIn capteur_lum(A0); 
PwmOut ma_led(A2); // LED connectée à A2 (contrôle PWM)
DigitalOut led1(LED1); // LED sur la carte de développement

BufferedSerial gps(D1, D0, 9600); // Communication série avec le GPS
BufferedSerial pc(USBTX, USBRX, 9600);

// Création d'un thread pour gérer LVGL (interface utilisateur)
ThreadLvgl threadLvgl(30);
float lecture_cap;

// Déclaration des objets LVGL
lv_obj_t *slider;
lv_obj_t *button;
lv_obj_t *switch_auto_gps;
lv_obj_t *switch_auto_intensite;
lv_obj_t *label_mode_gps;
lv_obj_t *label_mode_intensite;
lv_obj_t *label_state;
lv_obj_t *label_slider;
lv_obj_t *label_heure;

bool led_on = false; // État de la LED (allumée ou éteinte)
bool auto_mode_gps = false; // État du mode automatique gps
bool auto_mode_intensite= false;

// Tampon pour stocker les données GPS
char gpsBuffer[128];
int bufferIndex = 0;

// Fonction de callback pour le slider
void slider_event_cb(lv_event_t * e) {
    if (led_on && !auto_mode_gps) {
        int value = lv_slider_get_value(slider);
        ma_led.write(value / 100.0f); // Ajuste la luminosité de la LED
    }
}

// Fonction de callback pour le bouton
void button_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        led_on = !led_on; // Change l'état de la LED
        if (led_on) {
            lv_slider_set_value(slider, 100, LV_ANIM_OFF); // Met le slider à 100%
            ma_led.write(1.0); // Allume la LED à pleine luminosité
            lv_label_set_text(label_state, "LED: Allumée"); // Met à jour le label d'état
        } else {
            lv_slider_set_value(slider, 0, LV_ANIM_OFF); // Met le slider à 0%
            ma_led.write(0.0); // Éteint la LED
            lv_label_set_text(label_state, "LED: Éteinte"); // Met à jour le label d'état
        }
    }
}

// Fonction de callback pour le switch gps
void switch_event_gps(lv_event_t * e) {
    auto_mode_gps = lv_obj_has_state(switch_auto_gps, LV_STATE_CHECKED); // Change l'état du mode automatique
    if (auto_mode_gps) {
        lv_label_set_text(label_mode_gps, "Mode: Automatique avec Heure"); // Met à jour le label de mode
    } else {
        lv_label_set_text(label_mode_gps, "Mode: Manuel"); // Met à jour le label de mode
    }
}

// Fonction de callback pour le switch intensite
void switch_event_intensite(lv_event_t * e) {
    auto_mode_intensite = lv_obj_has_state(switch_auto_intensite, LV_STATE_CHECKED); // Change l'état du mode automatique
    if (auto_mode_intensite) {
        lv_label_set_text(label_mode_intensite, "Mode: Automatique en fonction de la luminosite"); // Met à jour le label de mode
    } else {
        lv_label_set_text(label_mode_intensite, "Mode: Manuel"); // Met à jour le label de mode
    }
}

// Configuration de l'interface utilisateur avec LVGL
void setup_ui() {
    lv_obj_t *scr = lv_scr_act(); // Obtient l'écran actuel

    // Crée et configure le bouton
    button = lv_btn_create(scr);
    lv_obj_set_size(button, 100, 50);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, -80);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "LED");
    lv_obj_center(label);

    // Crée et configure le slider
    slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 200);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Crée et configure les switch pour les mode automatique
    switch_auto_gps = lv_switch_create(scr);
    lv_obj_align(switch_auto_gps, LV_ALIGN_CENTER, -160, 80);
    lv_obj_add_event_cb(switch_auto_gps, switch_event_gps, LV_EVENT_VALUE_CHANGED, NULL);
    
    switch_auto_intensite = lv_switch_create(scr);
    lv_obj_align(switch_auto_intensite, LV_ALIGN_CENTER, 160, 80);
    lv_obj_add_event_cb(switch_auto_intensite, switch_event_intensite, LV_EVENT_VALUE_CHANGED, NULL);

    // Crée et configure les labels pour le mode et l'état de la LED
    label_mode_gps = lv_label_create(scr);
    lv_label_set_text(label_mode_gps, "Mode: Manuel Heure");
    lv_obj_align(label_mode_gps, LV_ALIGN_CENTER, -160, 100);

    label_mode_intensite = lv_label_create(scr);
    lv_label_set_text(label_mode_intensite, "Mode: Manuel intensite");
    lv_obj_align(label_mode_intensite, LV_ALIGN_CENTER, 160, 100);

    label_heure = lv_label_create(scr);
    lv_label_set_text(label_heure, "Heure programmee: ");
    lv_obj_align(label_heure, LV_ALIGN_CENTER, -160, 120);

    label_state = lv_label_create(scr);
    lv_label_set_text(label_state, "LED: Eteinte");
    lv_obj_align(label_state, LV_ALIGN_CENTER, 0, -50);

    label_slider = lv_label_create(scr);
    lv_label_set_text(label_slider, " Reglage d Intensite ");
    lv_obj_align(label_slider, LV_ALIGN_CENTER, 0, 20);
}



int main() {
    char gps_data;

    threadLvgl.lock();
    setup_ui(); 
    threadLvgl.unlock();

    ma_led.period_ms(1); 

    while (1) {
        lecture_cap = capteur_lum.read();

        if (auto_mode_intensite) 
        {
            ma_led.write(1.0-lecture_cap);
        }


        // Lecture des données GPS
        while (gps.readable()) 
        {
            gps.read(&gps_data, 1);

            if (gps_data == '\n') {
                gpsBuffer[bufferIndex] = '\0';

                // Vérification et extraction de l'heure si la trame est $GNRMC
                if (strncmp(gpsBuffer, "$GNRMC", 6) == 0) 
                {
                    int hour, minute, second;
                        if (sscanf(gpsBuffer, "$GNRMC,%2d%2d%2d", &hour, &minute, &second) == 3) {
                            char temps_propre[20];
                            snprintf(temps_propre, sizeof(temps_propre), "Heure GPS: %02d:%02d:%02d\r\n", hour+2, minute, second);
                            pc.write(temps_propre, strlen(temps_propre));
                            pc.write("Heure extraite avec succès\r\n", 29);
                            lv_label_set_text(label_heure, temps_propre);
                            if (auto_mode_gps) 
                            {
                                if (hour <= 18 ) {
                                ma_led.write(1.0);
                                lv_label_set_text(label_state, "LED: Allumee (Auto)");
                                } 

                            else {
                                    ma_led.write(0.0);
                                    lv_label_set_text(label_state, "LED: Eteinte (Auto)");
                                }
                            }
                        } 
                        else{
                                pc.write("Erreur de parsing de l'heure\r\n", 30);
                            }
                }

                // Réinitialiser le buffer pour la prochaine trame
                bufferIndex = 0;
            } 
            
            else if (gps_data != '\r' && bufferIndex < sizeof(gpsBuffer) - 1) {
                gpsBuffer[bufferIndex++] = gps_data;
            }

          
        }

  
    }

        ThisThread::sleep_for(500ms); // Attente de 500 ms avant de recommencer
}

