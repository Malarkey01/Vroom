/* =========================================================================
 *  VehicleInfoWindow.h — live OBD-II dashboard
 * -------------------------------------------------------------------------
 *  create_vehicle_info_window(parent)
 *      Opens a full-screen window that
 *          • spawns  Infotainment/scripts/obd_reader.py
 *          • retries every 10 s until data arrive
 *          • shows connection status (“Connecting” ↔ “Connected”)
 *          • displays eight key PIDs (RPM, SPEED …) in a 2-column grid
 *      The window owns the Python child process and cleans up on close.
 * ========================================================================= */
#ifndef VEHICLEINFOWINDOW_H
#define VEHICLEINFOWINDOW_H

#include <gtk/gtk.h>

GtkWidget *create_vehicle_info_window(GtkWindow *parent);

#endif /* VEHICLEINFOWINDOW_H */
