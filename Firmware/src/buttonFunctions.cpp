/*
This file holds all functions related to the buttons
*/

#include "wandCommander.h"
#include "buttonFunctions.h"

Button2 button1;
Button2 button2;

// Initialize buttons and set handlers
void buttonInit() {

    button1.begin(BUTTON_1_PIN);
    button1.setLongClickTime(1000); // 1 second for long click
    button1.setDoubleClickTime(500); // 0.5 second for double click

    button1.setClickHandler(click);
    button1.setDoubleClickHandler(doubleClick);
    button1.setTripleClickHandler(tripleClick);
    button1.setLongClickDetectedHandler(longClickDetected);
    button1.setLongClickHandler(longClick);
    button1.setLongClickDetectedRetriggerable(false);

    button2.begin(BUTTON_2_PIN);
    button2.setLongClickTime(1000); // 1 second for long click
    button2.setDoubleClickTime(500); // 0.5 second for double click

    button2.setClickHandler(click);
    button2.setDoubleClickHandler(doubleClick);
    button2.setTripleClickHandler(tripleClick);
    button2.setLongClickDetectedHandler(longClickDetected);
    button2.setLongClickHandler(longClick);
    button2.setLongClickDetectedRetriggerable(false);

}

void click(Button2& btn) {
    //LOG_DEBUG("Button %d: Click detected", btn.getPin());
    // switch case depending on which button is pressed
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            // Handle button 1 click
            LOG_DEBUG("Button 1 clicked");
            break;
        case BUTTON_2_PIN:
            // Handle button 2 click
            LOG_DEBUG("Button 2 clicked");
            break;
        default:
            break;
    }
}

void doubleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 double clicked");
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 double clicked");
            break;
        default:
            break;
    }
}

void tripleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 multiple click: ", btn.getNumberOfClicks());
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 multiple click: ", btn.getNumberOfClicks());
            break;
        default:
            break;
    }
}

