util_src = files([
  'util_env.cpp',
  'util_string.cpp',
  'util_fps_limiter.cpp',
  'util_flush.cpp',
  'util_gdi.cpp',
  'util_luid.cpp',
  'util_matrix.cpp',
  'util_monitor.cpp',
  'util_window.cpp',
  'util_json.cpp',
  'util_json.h',

  'thread.cpp',
  
  'util_shared_res.cpp',
  
  'com/com_guid.cpp',
  'com/com_private_data.cpp',

  'config/config.cpp',
  
  'log/metrics.cpp',
  'log/log.cpp',
  'log/log_debug.cpp',
  
  'sha1/sha1.c',
  'sha1/sha1_util.cpp',

  'sync/sync_recursive.cpp',
  'xxHash/xxhash.c',
  'xxHash/xxhash.h',

  'util_memoization.h',

  'util_messagechannel.cpp',
  'util_messagechannel.h',

  'util_singleton.h',

  'util_fastops.cpp',
  'util_fastops.h',

  'util_fast_cache.h',
  
  'util_filesys.h',
  'util_filesys.cpp',

  'util_threadpool.h',
  'util_atomic_queue.h',

  'util_renderprocessor.h',
  
  'util_version.h',

  'util_watchdog.h',
])

util_lib = static_library('util', util_src,
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

util_dep = declare_dependency(
  link_with           : [ util_lib ])
