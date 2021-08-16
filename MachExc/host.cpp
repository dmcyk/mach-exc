// mach
#include <dispatch/dispatch.h>
#include <dispatch/source.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/task.h>
#include <mach/thread_state.h>
#include <pthread.h>
#include <sys/socket.h>

// mach kernel
extern "C" {
#include "mach_exc_server.h"
}

// stl
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
kern_return_t catch_mach_exception_raise(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count) {
  printf("yes - handler\n");
  return KERN_SUCCESS;
}

kern_return_t catch_mach_exception_raise_state(
    mach_port_t exception_port,
    exception_type_t exception,
    const mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int *flavor,
    const thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
  printf("yes - handler state\n");
  return KERN_NOT_SUPPORTED;
}

void *recovery_point;
kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int *flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
  auto *state = (arm_thread_state64_t *)(old_state);

  printf("state, pc: %p\n", (void *)state->__pc);
  printf("set state to, pc: %p\n", recovery_point);

  auto *n_new_state = (arm_thread_state64_t *)(new_state);
  *n_new_state = *state;
  n_new_state->__pc = (uintptr_t)recovery_point;
  *new_state_count = ARM_THREAD_STATE64_COUNT;
  *flavor = ARM_THREAD_STATE64;

  return KERN_SUCCESS;
}

boolean_t mach_exc_server(
    mach_msg_header_t *InHeadP,
    mach_msg_header_t *OutHeadP);
}

void *entry_exc(void *port) {
  mach_msg_server(
      mach_exc_server, MACH_MSG_SIZE_RELIABLE, (mach_port_t)(ssize_t)port, 0);
  assert(false);
}

int __main(void) {
    mach_port_name_t exc_port;

    mach_port_t task = mach_task_self();
    if (mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port) !=
        KERN_SUCCESS) {
      return 1;
    }

    if (mach_port_insert_right(
            task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND) !=
        KERN_SUCCESS) {
      return 1;
    }

    // exc_server - default 32 bit handling
    // mach_exc_server - 32/64 bit - requires MACH_EXCEPTION_CODES
    auto ret = task_set_exception_ports(
        task,
        EXC_MASK_BAD_ACCESS | EXC_MASK_CRASH | EXC_MASK_ALL,
        exc_port,
        EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
        ARM_THREAD_STATE64);
    if (ret != KERN_SUCCESS) {
      printf("Failed exception ports: %d", ret);
      return 1;
    }

    printf("start exc server\n");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    pthread_create(&thread, &attr, entry_exc, (void *)(ssize_t)exc_port);
    pthread_attr_destroy(&attr);

  void *ptr = (void *)0xff;

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC),
      dispatch_get_main_queue(),
      ^() {
        printf("bad! %p\n", ptr);
        recovery_point = &&recovery_label;
        *(int *)(ptr) = 10;
        printf("how did we get here?\n");

      recovery_label:
        printf("recovered!\n");
        return;
      });

  return 0;
}
