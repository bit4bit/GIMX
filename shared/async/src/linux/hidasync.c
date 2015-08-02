/*
 Copyright (c) 2015 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include <hidasync.h>

#include <stdio.h>
#include <linux/hidraw.h>
#include <string.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <stdlib.h>

static int open_path(const char * path, int print) {
  
  int device = async_open_path(path, print);
  if(device >= 0) {
    struct hidraw_devinfo info;
    if(ioctl(devices[device].fd, HIDIOCGRAWINFO, &info) != -1) {
        devices[device].hid.vendor = info.vendor;
        devices[device].hid.product = info.product;
    }
    else {
        ASYNC_PRINT_ERROR("ioctl HIDIOCGRAWINFO")
        async_close(device);
        device = -1;
    }
  }
  return device;
}

/*
 * \brief Open a hid device.
 *
 * \param device_path  the path of the hid device to open.
 *
 * \return the identifier of the opened device (to be used in further operations), \
 * or -1 in case of failure (e.g. bad path, device already opened).
 */
int hidasync_open_path(const char * device_path) {

    return open_path(device_path, 1);
}

#define DEV "/dev"
#define HIDRAW_DEV_NAME "hidraw"

static int is_hidraw_device(const struct dirent *dir) {
  return strncmp(HIDRAW_DEV_NAME, dir->d_name, sizeof(HIDRAW_DEV_NAME)-1) == 0;
}

/*
 * \brief Open a hid device.
 *
 * \param vendor   the vendor id of the hid device to open.
 * \param product  the product id of the hid device to open.
 *
 * \return the identifier of the opened device (to be used in further operations), \
 * or -1 in case of failure (e.g. no device found).
 */
int hidasync_open_ids(unsigned short vendor, unsigned short product)
{
  int ret = -1;

  char path[sizeof("/dev/hidraw255")];
  struct dirent ** namelist_hidraw;
  int n_hidraw;
  int i;

  /*
   * TODO MLA: use libudev instead
   */

  // scan /dev for hidrawX devices
  n_hidraw = scandir(DEV, &namelist_hidraw, is_hidraw_device, alphasort);
  if (n_hidraw >= 0)
  {
    for (i = 0; i < n_hidraw; ++i)
    {
      snprintf(path, sizeof(path), "%s/%s", DEV, namelist_hidraw[i]->d_name);

      int device = open_path(path, 0);

      if(device >= 0) {
        if(devices[device].hid.vendor == vendor && devices[device].hid.product == product)
        {
          ret = device;
          break;
        }
        hidasync_close(device);
      }
    }
    for (i = 0; i < n_hidraw; ++i)
    {
      free(namelist_hidraw[i]);
    }
    free(namelist_hidraw);
  }

  return ret;
}

/*
 * \brief Get the USB ids of a hid device.
 *
 * \param device  the identifier of the hid device
 * \param vendor  where to store the vendor id
 * \param product where to store the product id
 *
 * \return 0 in case of success, or -1 in case of failure (i.e. bad device identifier).
 */
int hidasync_get_ids(int device, unsigned short * vendor, unsigned short * product) {

    ASYNC_CHECK_DEVICE(device)

    *vendor = devices[device].hid.vendor;
    *product = devices[device].hid.product;

    return 0;
}


/*
 * \brief Read from a hid device, with a timeout. Use this function in a synchronous context.
 *
 * \param device  the identifier of the hid device
 * \param buf     the buffer where to store the data
 * \param count   the maximum number of bytes to read
 * \param timeout the maximum time to wait, in seconds
 *
 * \return the number of bytes actually read
 */
int hidasync_read_timeout(int device, void * buf, unsigned int count, unsigned int timeout) {

  return async_read_timeout(device, buf, count, timeout);
}

/*
 * \brief Register the device as an event source, and set the external callbacks. \
 * This function triggers an asynchronous context. \
 * The fp_read callback is responsible for setting the next read size.
 *
 * \param device      the hid device
 * \param user        the user to pass to the external callback
 * \param fp_read     the external callback to call on data reception
 * \param fp_write    unused
 * \param fp_close    the external callback to call on failure
 * \param fp_register the function to register the device as an event source
 *
 * \return 0 in case of success, or -1 in case of error
 */
int hidasync_register(int device, int user, ASYNC_READ_CALLBACK fp_read, ASYNC_WRITE_CALLBACK fp_write, ASYNC_CLOSE_CALLBACK fp_close, ASYNC_REGISTER_SOURCE fp_register) {

    async_set_read_size(device, HIDASYNC_MAX_TRANSFER_SIZE);
    
    return async_register(device, user, fp_read, fp_write, fp_close, fp_register);
}

/*
 * \brief Write to a serial device, with a timeout. Use this function in a synchronous context.
 *
 * \param device  the identifier of the serial device
 * \param buf     the buffer containing the data to write
 * \param count   the number of bytes in buf
 * \param timeout the maximum time to wait for the completion, in seconds
 *
 * \return the number of bytes actually written (0 in case of timeout)
 */
int hidasync_write_timeout(int device, void * buf, unsigned int count, unsigned int timeout) {

    return async_write_timeout(device, buf, count, timeout);
}

/*
 * \brief Send data to a serial device. Use this function in an asynchronous context.
 *
 * \param device  the identifier of the serial device
 * \param buf     the buffer containing the data to send
 * \param count   the maximum number of bytes to send
 *
 * \return -1 in case of error, 0 in case of pending write, or the number of bytes written
 */
int hidasync_write(int device, const void * buf, unsigned int count) {

    return async_write(device, buf, count);
}

/*
 * \brief Close a hid device.
 *
 * \param device  the identifier of the hid device to close.
 *
 * \return 0 in case of success, or -1 in case of failure (i.e. bad device identifier).
 */
int hidasync_close(int device) {

    return async_close(device);
}
