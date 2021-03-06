/*
 Copyright (c) 2014 Mathieu Laurendeau
 License: GPLv3
 */

#ifndef REPORT_H_
#define REPORT_H_

#include <ds2.h>
#include <ds3.h>
#include <ds4.h>
#include <joystick.h>
#include <xbox.h>
#include <x360.h>
#include <xone.h>
#include <t300rs_ps4.h>
#include <g27_ps3.h>

#define MAX_REPORTS 2

typedef union
{
  s_report_joystick js;
  s_report_ds2 ds2;
  s_report_ds3 ds3;
  s_report_ds4 ds4;
  s_report_xbox xbox;
  s_report_x360 x360;
  s_report_xone xone;
  s_report_t300rsPs4 t300rsPs4;
  s_report_g27Ps3 g27Ps3;
} s_report;

typedef struct GIMX_PACKED
{
  unsigned char type;
  unsigned char length;
  s_report value;
} s_report_packet;

void report_register_builder(e_controller_type type, unsigned int (*fp)(int axis[AXIS_MAX], s_report_packet report[MAX_REPORTS]));
unsigned int report_build(e_controller_type type, int axis[AXIS_MAX], s_report_packet report[MAX_REPORTS]);

#endif /* REPORT_H_ */
