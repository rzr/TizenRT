/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "jerryscript-port-default.h"
#include "test-common.h"

/**
 * Register a JavaScript value in the global object.
 */
static void
register_js_value (const char *name_p, /**< name of the function */
                   jerry_value_t value) /**< JS value */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_set_property (global_obj_val, name_val, value);
  TEST_ASSERT (jerry_value_is_boolean (result_val));

  jerry_release_value (name_val);
  jerry_release_value (global_obj_val);

  jerry_release_value (result_val);
} /* register_js_value */

static jerry_value_t
assert_handler (const jerry_value_t func_obj_val, /**< function object */
                const jerry_value_t this_val, /**< this arg */
                const jerry_value_t args_p[], /**< function arguments */
                const jerry_length_t args_cnt) /**< number of function arguments */
{
  JERRY_UNUSED (func_obj_val);
  JERRY_UNUSED (this_val);

  if (args_cnt > 0
      && jerry_value_is_boolean (args_p[0])
      && jerry_get_boolean_value (args_p[0]))
  {
    return jerry_create_boolean (true);
  }

  if (args_cnt > 1
      && jerry_value_is_string (args_p[1]))
  {
    jerry_length_t utf8_sz = jerry_get_string_size (args_p[1]);
    char string_from_utf8[utf8_sz];
    string_from_utf8[utf8_sz] = 0;

    jerry_string_to_char_buffer (args_p[1], (jerry_char_t *) string_from_utf8, utf8_sz);

    printf ("JS assert: %s\n", string_from_utf8);
  }

  TEST_ASSERT (false);
} /* assert_handler */

/**
 * Checks whether global object has arraybuffer.
 */
static bool
arraybuffer_is_available (void)
{
  jerry_value_t global_obj_val = jerry_get_global_object ();
  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "ArrayBuffer");

  jerry_value_t prop_value = jerry_has_property (global_obj_val, prop_name);
  bool has_prop = jerry_get_boolean_value (prop_value);

  jerry_release_value (global_obj_val);
  jerry_release_value (prop_name);
  jerry_release_value (prop_value);

  return has_prop;
} /* arraybuffer_is_available */

/**
 * Test ArrayBuffer 'read' api call with various offset values.
 */
static void
test_read_with_offset (uint8_t offset) /**< offset for buffer read. */
{
  const char *eval_arraybuffer_src_p = ("var array = new Uint8Array (15);"
                                        "for (var i = 0; i < array.length; i++) { array[i] = i * 2; };"
                                        "array.buffer");
  jerry_value_t arraybuffer = jerry_eval ((jerry_char_t *) eval_arraybuffer_src_p,
                                          strlen (eval_arraybuffer_src_p),
                                          true);

  TEST_ASSERT (!jerry_value_has_error_flag (arraybuffer));
  TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
  TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == 15);

  uint8_t buffer[20];
  memset (buffer, 120, 20);

  /* Try to copy more than the target buffer size. */
  jerry_length_t copied = jerry_arraybuffer_read (arraybuffer, offset, buffer, 20);
  TEST_ASSERT (copied == (jerry_length_t)(15 - offset));

  for (uint8_t i = 0; i < copied; i++)
  {
    TEST_ASSERT (buffer[i] == (i + offset) * 2);
  }
  TEST_ASSERT (buffer[15 - offset] == 120);

  jerry_release_value (arraybuffer);
} /* test_read_with_offset */

/**
 * Test ArrayBuffer 'write' api call with various offset values.
 */
static void test_write_with_offset (uint8_t offset) /**< offset for buffer write. */
{
  {
    jerry_value_t offset_val = jerry_create_number (offset);
    register_js_value ("offset", offset_val);
    jerry_release_value (offset_val);
  }

  const char *eval_arraybuffer_src_p = "var array = new Uint8Array (15); array.buffer";
  jerry_value_t arraybuffer = jerry_eval ((jerry_char_t *) eval_arraybuffer_src_p,
                                          strlen (eval_arraybuffer_src_p),
                                          true);

  TEST_ASSERT (!jerry_value_has_error_flag (arraybuffer));
  TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
  TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == 15);

  uint8_t buffer[20];

  for (uint8_t i = 0; i < 20; i++)
  {
    buffer[i] = (uint8_t) (i * 3);
  }

  /* Intentionally copy more than the allowed space. */
  jerry_length_t copied = jerry_arraybuffer_write (arraybuffer, offset, buffer, 20);
  TEST_ASSERT (copied == (jerry_length_t)(15 - offset));

  const char *eval_test_arraybuffer_p = (
      "for (var i = 0; i < offset; i++)"
      "{"
      "  assert (array[i] == 0, 'offset check for: ' + i + ' was: ' + array[i] + ' should be: 0');"
      "};"
      "for (var i = offset; i < array.length; i++)"
      "{"
      "  var expected = (i - offset) * 3;"
      "  assert (array[i] == expected, 'calc check for: ' + i + ' was: ' + array[i] + ' should be: ' + expected);"
      "};"
      "assert (array[15] === undefined, 'ArrayBuffer out of bounds index should return undefined value');");
  jerry_value_t res = jerry_eval ((jerry_char_t *) eval_test_arraybuffer_p,
                                  strlen (eval_test_arraybuffer_p),
                                  true);
  jerry_release_value (res);
  jerry_release_value (arraybuffer);
} /* test_write_with_offset */

static bool callback_called = false;

static void test_free_cb (void *buffer) /**< buffer to free (if needed) */
{
  (void) buffer;
  callback_called = true;
} /* test_free_cb */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  if (!arraybuffer_is_available ())
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "ArrayBuffer is disabled!\n");
    jerry_cleanup ();
    return 0;
  }

  jerry_value_t function_val = jerry_create_external_function (assert_handler);
  register_js_value ("assert", function_val);
  jerry_release_value (function_val);

  /* Test array buffer queries */
  {
    const char *eval_arraybuffer_src_p = "new ArrayBuffer (10)";
    jerry_value_t eval_arraybuffer = jerry_eval ((jerry_char_t *) eval_arraybuffer_src_p,
                                                 strlen (eval_arraybuffer_src_p),
                                                 true);
    TEST_ASSERT (!jerry_value_has_error_flag (eval_arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (eval_arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (eval_arraybuffer) == 10);
    jerry_release_value (eval_arraybuffer);
  }

  /* Test array buffer creation */
  {
    const uint32_t length = 15;
    jerry_value_t arraybuffer = jerry_create_arraybuffer (length);
    TEST_ASSERT (!jerry_value_has_error_flag (arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == length);
    jerry_release_value (arraybuffer);
  }

  /* Test array buffer read operations */
  for (uint8_t i = 0; i < 15; i++)
  {
    test_read_with_offset (i);
  }

  /* Test zero length ArrayBuffer read */
  {
    const uint32_t length = 0;
    jerry_value_t arraybuffer = jerry_create_arraybuffer (length);
    TEST_ASSERT (!jerry_value_has_error_flag (arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == length);

    uint8_t data[20];
    memset (data, 11, 20);

    jerry_length_t bytes_read = jerry_arraybuffer_read (arraybuffer, 0, data, 20);
    TEST_ASSERT (bytes_read == 0);

    for (int i = 0; i < 20; i++)
    {
      TEST_ASSERT (data[i] == 11);
    }

    jerry_release_value (arraybuffer);
  }

  /* Test array buffer write operations */
  for (uint8_t i = 0; i < 15; i++)
  {
    test_write_with_offset (i);
  }

  /* Test zero length ArrayBuffer write */
  {
    const uint32_t length = 0;
    jerry_value_t arraybuffer = jerry_create_arraybuffer (length);
    TEST_ASSERT (!jerry_value_has_error_flag (arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == length);

    uint8_t data[20];
    memset (data, 11, 20);

    jerry_length_t bytes_written = jerry_arraybuffer_write (arraybuffer, 0, data, 20);
    TEST_ASSERT (bytes_written == 0);

    jerry_release_value (arraybuffer);
  }

  /* Test ArrayBuffer with buffer allocated externally */
  {
    const uint32_t buffer_size = 15;
    const uint8_t base_value = 51;

    uint8_t buffer_p[buffer_size];
    memset (buffer_p, base_value, buffer_size);

    jerry_value_t arrayb = jerry_create_arraybuffer_external (buffer_size, buffer_p, test_free_cb);
    uint8_t new_value = 123;
    jerry_length_t copied = jerry_arraybuffer_write (arrayb, 0, &new_value, 1);
    TEST_ASSERT (copied == 1);
    TEST_ASSERT (buffer_p[0] == new_value);
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arrayb) == buffer_size);

    for (uint32_t i = 1; i < buffer_size; i++)
    {
      TEST_ASSERT (buffer_p[i] == base_value);
    }

    uint8_t test_buffer[buffer_size];
    jerry_length_t read = jerry_arraybuffer_read (arrayb, 0, test_buffer, buffer_size);
    TEST_ASSERT (read == buffer_size);
    TEST_ASSERT (test_buffer[0] == new_value);

    for (uint32_t i = 1; i < buffer_size; i++)
    {
      TEST_ASSERT (test_buffer[i] == base_value);
    }

    TEST_ASSERT (jerry_value_is_arraybuffer (arrayb));
    jerry_release_value (arrayb);
  }

  /* Test ArrayBuffer external memory map/unmap */
  {
    const uint32_t buffer_size = 20;
    uint8_t buffer_p[buffer_size];
    {
      jerry_value_t input_buffer = jerry_create_arraybuffer_external (buffer_size, buffer_p, NULL);
      register_js_value ("input_buffer", input_buffer);
      jerry_release_value (input_buffer);
    }

    const char *eval_arraybuffer_src_p = (
      "var array = new Uint8Array(input_buffer);"
      "for (var i = 0; i < array.length; i++)"
      "{"
      "  array[i] = i * 2;"
      "};"
      "array.buffer");
    jerry_value_t buffer = jerry_eval ((jerry_char_t *) eval_arraybuffer_src_p,
                                        strlen (eval_arraybuffer_src_p),
                                        true);

    TEST_ASSERT (!jerry_value_has_error_flag (buffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (buffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (buffer) == 20);

    uint8_t *const data = jerry_get_arraybuffer_pointer (buffer);

    /* test memory read */
    for (int i = 0; i < 20; i++)
    {
      TEST_ASSERT (data[i] == (uint8_t) (i * 2));
    }

    /* "upload" new data */
    double sum = 0;
    for (int i = 0; i < 20; i++)
    {
      data[i] = (uint8_t)(i * 3);
      sum += data[i];
    }

    jerry_release_value (buffer);

    const char *eval_test_arraybuffer_p = (
      "var sum = 0;"
      "for (var i = 0; i < array.length; i++)"
      "{"
      "  var expected = i * 3;"
      "  assert(array[i] == expected, 'Array at index ' + i + ' was: ' + array[i] + ' should be: ' + expected);"
      "  sum += array[i]"
      "};"
      "sum");
    jerry_value_t res = jerry_eval ((jerry_char_t *) eval_test_arraybuffer_p,
                                    strlen (eval_test_arraybuffer_p),
                                    true);
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == sum);
    jerry_release_value (res);

    jerry_release_value (buffer);
  }

  /* Test ArrayBuffer external with invalid arguments */
  {
    jerry_value_t input_buffer = jerry_create_arraybuffer_external (0, NULL, NULL);
    TEST_ASSERT (jerry_value_has_error_flag (input_buffer));
    TEST_ASSERT (jerry_get_error_type (input_buffer) == JERRY_ERROR_RANGE);
    jerry_release_value (input_buffer);
  }

  jerry_cleanup ();

  TEST_ASSERT (callback_called == true);
} /* main */
