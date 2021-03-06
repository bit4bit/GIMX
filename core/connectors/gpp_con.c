/*
 Copyright (c) 2012 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include <string.h>
#include "../../shared/gpp/pcprog.h"
#include "config.h"
#include <controller2.h>

int gpp_connect(int id, const char* device)
{
  int ret = -1;
  GCAPI_REPORT report;

  if(gppcprog_connect(id, device) != 1)
  {
    return -1;
  }

  int i;
  for(i = 0; i < 10; ++i)
  {
    if(gpppcprog_input(id, &report, 100) == 1)
    {
      break;
    }
  }

  if(i == 10)
  {
    return -1;
  }
  
  switch(report.console)
  {
    case CONSOLE_DISCONNECTED:
      ret = -1;
      break;
    case CONSOLE_PS3:
      ret = C_TYPE_SIXAXIS;
      break;
    case CONSOLE_XB360:
      ret = C_TYPE_360_PAD;
      break;
    case CONSOLE_PS4:
      ret = C_TYPE_DS4;
      break;
    case CONSOLE_XB1:
      ret = C_TYPE_XONE_PAD;
      break;
  }

  return ret;
}

inline int8_t scale_axis(e_controller_type type, int index, int axis[AXIS_MAX])
{
  return axis[index] * 100 / controller_get_max_signed(type, index);
}

int gpp_send(int id, e_controller_type type, int axis[AXIS_MAX])
{
  int8_t output[GCAPI_INPUT_TOTAL] = {};
  int axis_value;
  int ret = 0;

  output[PS3_UP] = scale_axis(type, sa_up, axis);
  output[PS3_DOWN] = scale_axis(type, sa_down, axis);
  output[PS3_LEFT] = scale_axis(type, sa_left, axis);
  output[PS3_RIGHT] = scale_axis(type, sa_right, axis);
  output[PS3_START] = axis[sa_start] ? 100 : 0;
  output[PS3_SELECT] = axis[sa_select] ? 100 : 0;
  output[PS3_L3] = scale_axis(type, sa_l3, axis);
  output[PS3_R3] = scale_axis(type, sa_r3, axis);
  output[PS3_L1] = scale_axis(type, sa_l1, axis);
  output[PS3_R1] = scale_axis(type, sa_r1, axis);
  output[PS3_PS] = axis[sa_ps] ? 100 : 0;
  output[PS3_CROSS] = scale_axis(type, sa_cross, axis);
  output[PS3_CIRCLE] = scale_axis(type, sa_circle, axis);
  output[PS3_SQUARE] = scale_axis(type, sa_square, axis);
  output[PS3_TRIANGLE] = scale_axis(type, sa_triangle, axis);
  output[PS3_L2] = scale_axis(type, sa_l2, axis);
  output[PS3_R2] = scale_axis(type, sa_r2, axis);

  axis_value = scale_axis(type, sa_lstick_x, axis);
  output[PS3_LX] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_lstick_y, axis);
  output[PS3_LY] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_rstick_x, axis);
  output[PS3_RX] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_rstick_y, axis);
  output[PS3_RY] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_acc_x, axis);
  output[PS3_ACCX] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_acc_y, axis);
  output[PS3_ACCY] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_acc_z, axis);
  output[PS3_ACCZ] = clamp(-100, axis_value, 100);

  axis_value = scale_axis(type, sa_gyro, axis);
  output[PS3_GYRO] = clamp(-100, axis_value, 100);

  output[PS4_TOUCH] = axis[ds4a_finger1] ? 100 : 0;
  output[PS4_TOUCHX] = scale_axis(type, ds4a_finger1_x, axis);
  output[PS4_TOUCHY] = scale_axis(type, ds4a_finger1_y, axis);

  if(!gpppcprog_output(id, output))
  {
    ret = -1;
  }

  return ret;
}

void gpp_disconnect(int id)
{
  gppcprog_disconnect(id);
}
