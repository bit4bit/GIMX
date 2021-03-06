/*
 Copyright (c) 2010 Mathieu Laurendeau <mat.lau@laposte.net>
 Copyright (c) 2009 Jim Paris <jim@jtan.com>
 License: GPLv3
 */

#include <stdio.h> //fprintf
#include <locale.h> //internationalization
#include <prio.h> //to set the thread priority
#include <signal.h> //to catch SIGINT
#include <errno.h> //to print errors
#include <string.h> //to print errors

#ifndef WIN32
#include <pwd.h> //to get the homedir
#include <sys/types.h> //to get the homedir
#include <unistd.h> //to get the homedir
#else
#include <winsock2.h>
#include <windows.h>
#include <shlobj.h> //to get the homedir
#include <unistd.h> //usleep
#endif

#include "gimx.h"
#include "macros.h"
#include "config_reader.h"
#include "calibration.h"
#include "display.h"
#include "mainloop.h"
#include "connectors/connector.h"
#include "connectors/bluetooth/bt_abs.h"
#include "args.h"
#include <adapter.h>
#include <stats.h>
#include <pcprog.h>
#include "../directories.h"

#define DEFAULT_POSTPONE_COUNT 3 //unit = DEFAULT_REFRESH_PERIOD

s_gimx_params gimx_params =
{
  .homedir = NULL,
  .force_updates = 0,
  .keygen = NULL,
  .grab = 1,
  .refresh_period = -1,
  .frequency_scale = 1,
  .status = 0,
  .curses = 0,
  .config_file = NULL,
  .postpone_count = DEFAULT_POSTPONE_COUNT,
  .subpositions = 0,
  .window_events = 0,
  .btstack = 0,
};

#ifdef WIN32
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch(dwType) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:

      set_done();//signal the main thread to terminate

      //Returning would make the process exit!
      //We just make the handler sleep until the main thread exits,
      //or until the maximum execution time for this handler is reached.
      Sleep(10000);

      return TRUE;
    default:
      break;
    }
    return FALSE;
}
#endif

void terminate(int sig)
{
  set_done();
}

int ignore_event(GE_Event* event)
{
  return 0;
}

int process_event(GE_Event* event)
{
  switch (event->type)
  {
    case GE_MOUSEMOTION:
      cfg_process_motion_event(event);
      break;
    case GE_JOYRUMBLE:
      cfg_process_rumble_event(event);
      break;
    default:
      if (!cal_skip_event(event))
      {
        cfg_process_event(event);
      }
      break;
  }

  //make sure to process the event before these two lines
  cfg_trigger_lookup(event);
  cfg_intensity_lookup(event);

  switch (event->type)
  {
    case GE_MOUSEBUTTONDOWN:
      cal_button(event->button.which, event->button.button);
      break;
    case GE_KEYDOWN:
      cal_key(event->key.which, event->key.keysym, 1);
      break;
    case GE_KEYUP:
      cal_key(event->key.which, event->key.keysym, 0);
      break;
  }

  if(event->type != GE_MOUSEMOTION)
  {
    macro_lookup(event);
  }

  return 0;
}

int main(int argc, char *argv[])
{
  GE_Event kgevent = {.type = GE_KEYDOWN};

  (void) signal(SIGINT, terminate);
  (void) signal(SIGTERM, terminate);
#ifndef WIN32
  (void) signal(SIGHUP, terminate);
#else
  if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE))
  {
    fprintf(stderr, "Unable to install handler!\n");
    exit(-1);
  }
#endif

  setlocale( LC_ALL, "" );
#ifndef WIN32
  bindtextdomain( "gimx", "/usr/share/locale" );
#else
  bindtextdomain( "gimx", "share/locale" );
#endif
  textdomain( "gimx" );

  setlocale( LC_NUMERIC, "C" ); /* Make sure we use '.' to write doubles. */
  
#ifndef WIN32
  setlinebuf(stdout);

  gimx_params.homedir = getpwuid(getuid())->pw_dir;
#else
  static char path[MAX_PATH];
  if(SHGetFolderPath( NULL, CSIDL_APPDATA , NULL, 0, path ))
  {
    fprintf(stderr, "Can't get the user directory.\n");
    goto QUIT;
  }
  gimx_params.homedir = path;
#endif

  if( set_prio() < 0 )
  {
    printf("Warning: failed to set process priority\n");
  }

  adapter_init();

  serial_init();

  gpppcprog_read_user_ids(gimx_params.homedir, GIMX_DIR);

  if(args_read(argc, argv, &gimx_params) < 0)
  {
    fprintf(stderr, _("Wrong argument.\n"));
    goto QUIT;
  }

  if(gimx_params.btstack)
  {
    bt_abs_value = E_BT_ABS_BTSTACK;
  }

  if(connector_init() < 0)
  {
    fprintf(stderr, _("connector_init failed\n"));
    goto QUIT;
  }

  if(gimx_params.refresh_period == -1)
  {
    /*
     * TODO MLA: per controller refresh period?
     */
    gimx_params.refresh_period = controller_get_default_refresh_period(adapter_get(0)->type);
    gimx_params.postpone_count = 3 * DEFAULT_REFRESH_PERIOD / gimx_params.refresh_period;
    printf(_("using default refresh period: %.02fms\n"), (double)gimx_params.refresh_period/1000);
  }
  else if(gimx_params.refresh_period < controller_get_min_refresh_period(adapter_get(0)->type))
  {
    fprintf(stderr, "Refresh period should be at least %.02fms\n", (double)controller_get_min_refresh_period(adapter_get(0)->type)/1000);
    goto QUIT;
  }

  if(gimx_params.curses)
  {
    display_init();
    stats_init(0);
  }

  gimx_params.frequency_scale = (double) DEFAULT_REFRESH_PERIOD / gimx_params.refresh_period;

  /*
   * The --event argument makes gimx send a packet and exit.
   */
  int event = 0;
  unsigned char controller;
  for(controller=0; controller<MAX_CONTROLLERS; ++controller)
  {
    if(adapter_get(controller)->event)
    {
      adapter_get(controller)->send_command = 1;
      event = 1;
    }
  }
  if(event)
  {
    connector_send();
    goto QUIT;
  }

  unsigned char src = GE_MKB_SOURCE_PHYSICAL;

  if(gimx_params.window_events)
  {
    src = GE_MKB_SOURCE_WINDOW_SYSTEM;
  }

  //TODO MLA: if there is no config file:
  // - there's no need to read macros
  // - there's no need to read inputs
  // - there's no need to grab the mouse
  if (!GE_initialize(src))
  {
    fprintf(stderr, _("GE_initialize failed\n"));
    goto QUIT;
  }

  if(gimx_params.grab)
  {
    GE_grab();
  }

  if(gimx_params.config_file)
  {
    cal_init();

    cfg_intensity_init();

    if(read_config_file(gimx_params.config_file) < 0)
    {
      fprintf(stderr, _("read_config_file failed\n"));
      goto QUIT;
    }

    if(GE_GetMKMode() == GE_MK_MODE_SINGLE_INPUT)
    {
      cfg_clean();
      GE_FreeMKames();

      cal_init();

      cfg_intensity_init();

      read_config_file(gimx_params.config_file);
    }

    cfg_read_calibration();
  }

  GE_release_unused();

  macros_init();

  if(gimx_params.keygen)
  {
    kgevent.key.keysym = GE_KeyId(gimx_params.keygen);
    if(kgevent.key.keysym)
    {
      macro_lookup(&kgevent);
    }
    else
    {
      fprintf(stderr, _("Unknown key name for argument --keygen: '%s'\n"), gimx_params.keygen);
      goto QUIT;
    }
  }

  cfg_trigger_init();

  mainloop();

  gprintf(_("Exiting\n"));

  QUIT:

  macros_clean();
  cfg_clean();
  GE_quit();
  connector_clean();

  xmlCleanupParser();

  if(gimx_params.curses)
  {
    display_end();
  }

  return 0;
}
