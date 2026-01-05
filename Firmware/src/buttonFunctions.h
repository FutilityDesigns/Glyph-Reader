#ifndef BUTTON_FUNCTIONS_H
#define BUTTON_FUNCTIONS_H
#include "Button2.h"

extern Button2 button1;
extern Button2 button2;

void buttonInit();
void click(Button2& btn);
void doubleClick(Button2& btn);
void tripleClick(Button2& btn);
void longClickDetected(Button2& btn);
void longClick(Button2& btn);

#endif