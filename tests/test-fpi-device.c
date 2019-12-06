/*
 * Example fingerprint device prints listing and deletion
 * Enrolls your right index finger and saves the print to disk
 * Copyright (C) 2019 Marco Trevisan <marco.trevisan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libfprint/fprint.h>

#include "fpi-device.h"
#include "test-device-fake.h"

typedef FpDevice FpAutoCloseDevice;

static FpAutoCloseDevice *
auto_close_fake_device_new (void)
{
  FpAutoCloseDevice *device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);

  g_assert_true (fp_device_open_sync (device, NULL, NULL));

  return device;
}

static void
auto_close_fake_device_free (FpAutoCloseDevice *device)
{
  if (fp_device_is_open (device))
    g_assert_true (fp_device_close_sync (device, NULL, NULL));

  g_object_unref (device);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (FpAutoCloseDevice, auto_close_fake_device_free)

typedef FpDeviceClass FpAutoResetClass;
static FpAutoResetClass default_fake_dev_class = {0};

static FpAutoResetClass *
auto_reset_device_class (void)
{
  g_autoptr(GTypeClass) type_class = NULL;
  FpDeviceClass *dev_class = g_type_class_peek_static (FPI_TYPE_DEVICE_FAKE);

  if (!dev_class)
    {
      type_class = g_type_class_ref (FPI_TYPE_DEVICE_FAKE);
      dev_class = (FpDeviceClass *) type_class;
      g_assert_nonnull (dev_class);
    }

  default_fake_dev_class = *dev_class;

  return dev_class;
}

static void
auto_reset_device_class_cleanup (FpAutoResetClass *dev_class)
{
  *dev_class = default_fake_dev_class;

  g_assert_cmpint (memcmp (dev_class, &default_fake_dev_class,
                           sizeof (FpAutoResetClass)), ==, 0);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (FpAutoResetClass, auto_reset_device_class_cleanup)


static void
on_driver_probe_async (GObject *initable, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpDevice) device = NULL;
  FpDeviceClass *dev_class;
  FpiDeviceFake *fake_dev;
  gboolean *done = user_data;

  device = FP_DEVICE (g_async_initable_new_finish (G_ASYNC_INITABLE (initable), res, &error));
  dev_class = FP_DEVICE_GET_CLASS (device);
  fake_dev = FPI_DEVICE_FAKE (device);

  g_assert (fake_dev->last_called_function == dev_class->probe);
  g_assert_no_error (error);

  g_assert_false (fp_device_is_open (device));

  *done = TRUE;
}

static void
test_driver_probe (void)
{
  gboolean done = FALSE;

  g_async_initable_new_async (FPI_TYPE_DEVICE_FAKE, G_PRIORITY_DEFAULT, NULL,
                              on_driver_probe_async, &done, NULL);

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

static void
test_driver_open (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpDevice) device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  g_assert (fake_dev->last_called_function != dev_class->probe);

  fp_device_open_sync (device, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->open);
  g_assert_no_error (error);
  g_assert_true (fp_device_is_open (device));

  fp_device_close_sync (FP_DEVICE (device), NULL, &error);
  g_assert_no_error (error);
}

static void
test_driver_open_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpDevice) device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  fp_device_open_sync (device, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->open);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert_false (fp_device_is_open (device));
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));
}

static void
test_driver_close (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  fp_device_close_sync (device, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->close);

  g_assert_no_error (error);
  g_assert_false (fp_device_is_open (device));
}

static void
test_driver_close_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  fp_device_close_sync (device, NULL, &error);

  g_assert (fake_dev->last_called_function == dev_class->close);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));
}

static void
test_driver_enroll (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *template_print = fp_print_new (device);
  FpPrint *out_print = NULL;

  out_print =
    fp_device_enroll_sync (device, template_print, NULL, NULL, NULL, &error);

  g_assert (fake_dev->last_called_function == dev_class->enroll);
  g_assert (fake_dev->action_data == template_print);

  g_assert_no_error (error);
  g_assert (out_print == template_print);
}

static void
test_driver_enroll_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *template_print = fp_print_new (device);
  FpPrint *out_print = NULL;

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  out_print =
    fp_device_enroll_sync (device, template_print, NULL, NULL, NULL, &error);

  g_assert (fake_dev->last_called_function == dev_class->enroll);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));
  g_assert_null (out_print);
}

static void
test_driver_verify (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *out_print = NULL;
  gboolean match;

  fake_dev->ret_result = FPI_MATCH_SUCCESS;
  fp_device_verify_sync (device, enrolled_print, NULL, &match, &out_print, &error);

  g_assert (fake_dev->last_called_function == dev_class->verify);
  g_assert (fake_dev->action_data == enrolled_print);
  g_assert_no_error (error);

  g_assert (out_print == enrolled_print);
  g_assert_true (match);
}

static void
test_driver_verify_fail (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *out_print = NULL;
  gboolean match;

  fake_dev->ret_result = FPI_MATCH_FAIL;
  fp_device_verify_sync (device, enrolled_print, NULL, &match, &out_print, &error);

  g_assert (fake_dev->last_called_function == dev_class->verify);
  g_assert_no_error (error);

  g_assert (out_print == enrolled_print);
  g_assert_false (match);
}

static void
test_driver_verify_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *out_print = NULL;
  gboolean match;

  fake_dev->ret_result = FPI_MATCH_ERROR;
  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  fp_device_verify_sync (device, enrolled_print, NULL, &match, &out_print, &error);

  g_assert (fake_dev->last_called_function == dev_class->verify);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));
  g_assert_false (match);
}

static void
fake_device_stub_identify (FpDevice *device)
{
}

static void
test_driver_supports_identify (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->identify = fake_device_stub_identify;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_true (fp_device_supports_identify (device));
}

static void
test_driver_do_not_support_identify (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->identify = NULL;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_false (fp_device_supports_identify (device));
}

static void
test_driver_identify (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpPrint) print = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(GPtrArray) prints = g_ptr_array_new_with_free_func (g_object_unref);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *matched_print;
  FpPrint *expected_matched;
  unsigned int i;

  for (i = 0; i < 500; ++i)
    g_ptr_array_add (prints, fp_print_new (device));

  expected_matched = g_ptr_array_index (prints, g_random_int_range (0, 499));
  fp_print_set_description (expected_matched, "fake-verified");

  g_assert_true (fp_device_supports_identify (device));

  fake_dev->ret_print = fp_print_new (device);
  fp_device_identify_sync (device, prints, NULL, &matched_print, &print, &error);

  g_assert (fake_dev->last_called_function == dev_class->identify);
  g_assert (fake_dev->action_data == prints);
  g_assert_no_error (error);

  g_assert (print != NULL && print == fake_dev->ret_print);
  g_assert (expected_matched == matched_print);
}

static void
test_driver_identify_fail (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpPrint) print = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(GPtrArray) prints = g_ptr_array_new_with_free_func (g_object_unref);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *matched_print;
  unsigned int i;

  for (i = 0; i < 500; ++i)
    g_ptr_array_add (prints, fp_print_new (device));

  g_assert_true (fp_device_supports_identify (device));

  fake_dev->ret_print = fp_print_new (device);
  fp_device_identify_sync (device, prints, NULL, &matched_print, &print, &error);

  g_assert (fake_dev->last_called_function == dev_class->identify);
  g_assert_no_error (error);

  g_assert (print != NULL && print == fake_dev->ret_print);
  g_assert_null (matched_print);
}

static void
test_driver_identify_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpPrint) print = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(GPtrArray) prints = g_ptr_array_new_with_free_func (g_object_unref);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpPrint *matched_print;
  FpPrint *expected_matched;
  unsigned int i;

  for (i = 0; i < 500; ++i)
    g_ptr_array_add (prints, fp_print_new (device));

  expected_matched = g_ptr_array_index (prints, g_random_int_range (0, 499));
  fp_print_set_description (expected_matched, "fake-verified");

  g_assert_true (fp_device_supports_identify (device));

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  fp_device_identify_sync (device, prints, NULL, &matched_print, &print, &error);

  g_assert (fake_dev->last_called_function == dev_class->identify);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));
  g_assert_null (matched_print);
  g_assert_null (print);
}

static void
fake_device_stub_capture (FpDevice *device)
{
}

static void
test_driver_supports_capture (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->capture = fake_device_stub_capture;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_true (fp_device_supports_capture (device));
}

static void
test_driver_do_not_support_capture (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->capture = NULL;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_false (fp_device_supports_capture (device));
}

static void
test_driver_capture (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpImage) image = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  gboolean wait_for_finger = TRUE;

  fake_dev->ret_image = fp_image_new (500, 500);
  image = fp_device_capture_sync (device, wait_for_finger, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->capture);
  g_assert_true (GPOINTER_TO_UINT (fake_dev->action_data));
  g_assert_no_error (error);

  g_assert (image == fake_dev->ret_image);
}

static void
test_driver_capture_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpImage) image = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  gboolean wait_for_finger = TRUE;

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  image = fp_device_capture_sync (device, wait_for_finger, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->capture);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));

  g_assert_null (image);
}

static void
fake_device_stub_list (FpDevice *device)
{
}

static void
test_driver_has_storage (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->list = fake_device_stub_list;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_true (fp_device_has_storage (device));
}

static void
test_driver_has_not_storage (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpDevice) device = NULL;

  dev_class->list = NULL;

  device = g_object_new (FPI_TYPE_DEVICE_FAKE, NULL);
  g_assert_false (fp_device_has_storage (device));
}

static void
test_driver_list (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  g_autoptr(GPtrArray) prints = g_ptr_array_new_with_free_func (g_object_unref);
  unsigned int i;

  for (i = 0; i < 500; ++i)
    g_ptr_array_add (prints, fp_print_new (device));

  fake_dev->ret_list = g_steal_pointer (&prints);
  prints = fp_device_list_prints_sync (device, NULL, &error);

  g_assert (fake_dev->last_called_function == dev_class->list);
  g_assert_no_error (error);

  g_assert (prints == fake_dev->ret_list);
}

static void
test_driver_list_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  g_autoptr(GPtrArray) prints = NULL;

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  prints = fp_device_list_prints_sync (device, NULL, &error);

  g_assert (fake_dev->last_called_function == dev_class->list);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));

  g_assert_null (prints);
}

static void
test_driver_delete (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  gboolean ret;

  ret = fp_device_delete_print_sync (device, enrolled_print, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->delete);
  g_assert (fake_dev->action_data == enrolled_print);
  g_assert_no_error (error);
  g_assert_true (ret);
}

static void
test_driver_delete_error (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  gboolean ret;

  fake_dev->ret_error = fpi_device_error_new (FP_DEVICE_ERROR_GENERAL);
  ret = fp_device_delete_print_sync (device, enrolled_print, NULL, &error);
  g_assert (fake_dev->last_called_function == dev_class->delete);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL);
  g_assert (error == g_steal_pointer (&fake_dev->ret_error));

  g_assert_false (ret);
}

static gboolean
fake_device_delete_wait_for_cancel_timeout (gpointer data)
{
  FpDevice *device = data;
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);

  g_assert (fake_dev->last_called_function == dev_class->cancel);
  default_fake_dev_class.delete (device);

  g_assert (fake_dev->last_called_function == default_fake_dev_class.delete);
  fake_dev->last_called_function = fake_device_delete_wait_for_cancel_timeout;

  return G_SOURCE_REMOVE;
}

static void
fake_device_delete_wait_for_cancel (FpDevice *device)
{
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  fake_dev->last_called_function = fake_device_delete_wait_for_cancel;

  g_timeout_add (100, fake_device_delete_wait_for_cancel_timeout, device);
}

static void
on_driver_cancel_delete (GObject *obj, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  FpDevice *device = FP_DEVICE (obj);
  gboolean *completed = user_data;

  fp_device_delete_print_finish (device, res, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  *completed = TRUE;
}

static void
test_driver_cancel (void)
{
  g_autoptr(FpAutoResetClass) dev_class = auto_reset_device_class ();
  g_autoptr(FpAutoCloseDevice) device = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(FpPrint) enrolled_print = NULL;
  gboolean completed = FALSE;
  FpiDeviceFake *fake_dev;

  dev_class->delete = fake_device_delete_wait_for_cancel;

  device = auto_close_fake_device_new ();
  fake_dev = FPI_DEVICE_FAKE (device);
  cancellable = g_cancellable_new ();
  enrolled_print = fp_print_new (device);

  fp_device_delete_print (device, enrolled_print, cancellable,
                          on_driver_cancel_delete, &completed);
  g_cancellable_cancel (cancellable);

  while (!completed)
    g_main_context_iteration (NULL, TRUE);

  g_assert (fake_dev->last_called_function == fake_device_delete_wait_for_cancel_timeout);
}

static void
test_driver_no_cancel (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FpAutoCloseDevice) device = auto_close_fake_device_new ();
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(FpPrint) enrolled_print = fp_print_new (device);
  FpDeviceClass *dev_class = FP_DEVICE_GET_CLASS (device);
  FpiDeviceFake *fake_dev = FPI_DEVICE_FAKE (device);

  fp_device_delete_print_sync (device, enrolled_print, cancellable, &error);
  g_assert (fake_dev->last_called_function == dev_class->delete);
  g_cancellable_cancel (cancellable);

  while (g_main_context_iteration (NULL, FALSE))
    ;

  g_assert (fake_dev->last_called_function == dev_class->delete);
  g_assert_no_error (error);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/driver/supports_identify", test_driver_supports_identify);
  g_test_add_func ("/driver/supports_capture", test_driver_supports_capture);
  g_test_add_func ("/driver/has_storage", test_driver_has_storage);
  g_test_add_func ("/driver/do_not_support_identify", test_driver_do_not_support_identify);
  g_test_add_func ("/driver/do_not_support_capture", test_driver_do_not_support_capture);
  g_test_add_func ("/driver/has_not_storage", test_driver_has_not_storage);

  g_test_add_func ("/driver/probe", test_driver_probe);
  g_test_add_func ("/driver/open", test_driver_open);
  g_test_add_func ("/driver/open/error", test_driver_open_error);
  g_test_add_func ("/driver/close", test_driver_close);
  g_test_add_func ("/driver/close/error", test_driver_close_error);
  g_test_add_func ("/driver/enroll", test_driver_enroll);
  g_test_add_func ("/driver/enroll/error", test_driver_enroll_error);
  g_test_add_func ("/driver/verify", test_driver_verify);
  g_test_add_func ("/driver/verify/fail", test_driver_verify_fail);
  g_test_add_func ("/driver/verify/error", test_driver_verify_error);
  g_test_add_func ("/driver/identify", test_driver_identify);
  g_test_add_func ("/driver/identify/fail", test_driver_identify_fail);
  g_test_add_func ("/driver/identify/error", test_driver_identify_error);
  g_test_add_func ("/driver/capture", test_driver_capture);
  g_test_add_func ("/driver/capture/error", test_driver_capture_error);
  g_test_add_func ("/driver/list", test_driver_list);
  g_test_add_func ("/driver/list/error", test_driver_list_error);
  g_test_add_func ("/driver/delete", test_driver_delete);
  g_test_add_func ("/driver/delete/error", test_driver_delete_error);
  g_test_add_func ("/driver/no-cancel", test_driver_no_cancel);
  g_test_add_func ("/driver/cancel", test_driver_cancel);

  return g_test_run ();
}
