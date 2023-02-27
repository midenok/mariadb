message("* Configuring ServiceNow build...")
# Flags used by the compiler during SNOW builds
SET(COMPILER_FLAGS_SNOW
    "-Wa,-mbranches-within-32B-boundaries -DDBUG_OFF ${COMPILER_FLAGS_SNOW}")
# Flags used by the linker during SNOW builds
SET(LINKER_FLAGS_SNOW
    "-z relro -z now ${LINKER_FLAGS_SNOW}")
SET(CMAKE_CXX_FLAGS_SNOW
    "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_FLAGS_SNOW}"
    CACHE STRING "Flags used by C++ compiler during SNOW builds"
    FORCE)
SET(CMAKE_C_FLAGS_SNOW
    "${CMAKE_C_FLAGS_RELEASE} ${COMPILER_FLAGS_SNOW}"
    CACHE STRING "Flags used by C compiler during SNOW builds"
    FORCE)
SET(CMAKE_ASM_FLAGS_SNOW
    "${CMAKE_ASM_FLAGS_RELEASE} ${COMPILER_FLAGS_SNOW}"
    CACHE STRING "Flags used by assembler during SNOW builds"
    FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_SNOW
    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${LINKER_FLAGS_SNOW}"
    CACHE STRING "Flags used for linking binaries during SNOW builds"
    FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS_SNOW
    "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${LINKER_FLAGS_SNOW}"
    CACHE STRING "Flags used by shared libraries linker during SNOW builds"
    FORCE)
SET(CMAKE_STATIC_LINKER_FLAGS_SNOW
    "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}"
    CACHE STRING "Flags used by static libraries linker during SNOW builds"
    FORCE)
SET(CMAKE_MODULE_LINKER_FLAGS_SNOW
    "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${LINKER_FLAGS_SNOW}"
    CACHE STRING "Flags used by module libraries linker during SNOW builds"
    FORCE)
SET(CMAKE_BUILD_TYPE SNOW
    CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo MinSizeRel SNOW"
    FORCE)
MARK_AS_ADVANCED(FORCE
    CMAKE_CXX_FLAGS_SNOW
    CMAKE_C_FLAGS_SNOW
    CMAKE_ASM_FLAGS_SNOW
    CMAKE_EXE_LINKER_FLAGS_SNOW
    CMAKE_SHARED_LINKER_FLAGS_SNOW
    CMAKE_STATIC_LINKER_FLAGS_SNOW
    CMAKE_MODULE_LINKER_FLAGS_SNOW)

# TODO: if cached variable not yet exists it will have empty HELPSTRING,
# we don't duplicate them there. CMake is very limited on overriding cached
# variables with normal variables (-D and normal variables work differently).

macro(setc var value)
  if("$CACHE{${var}}" STREQUAL "")
    set (extra_args ${ARGN})
    list(GET extra_args 0 type)
    if ("${type}" STREQUAL "NOTFOUND")
      set(${var} "${value}" CACHE BOOL "" FORCE)
    else()
      set(${var} "${value}" CACHE ${type} "" FORCE)
    endif()
  else()
    get_property(doc CACHE ${var} PROPERTY HELPSTRING)
    get_property(type CACHE ${var} PROPERTY TYPE)
    set(${var} "${value}" CACHE ${type} "${doc}" FORCE)
  endif()
endmacro()

# Default type is BOOL. Must specify type for non-yet-existent cached variables.
# This could be much better if normal variables worked like -D
setc(DISABLE_SHARED OFF)
setc(ENABLED_PROFILING OFF)
setc(ENABLE_DTRACE OFF)
setc(MAX_INDEXES 128 STRING)
setc(MUTEXTYPE futex STRING)
setc(SECURITY_HARDENED OFF)
setc(MYSQL_MAINTAINER_MODE NO STRING)
setc(CONC_WITH_DYNCOL NO)
setc(CONC_WITH_EXTERNAL_ZLIB NO)
setc(CONC_WITH_MYSQLCOMPAT NO)
setc(CONC_WITH_UNIT_TESTS NO)
setc(GSSAPI_FOUND FALSE)
setc(UPDATE_SUBMODULES OFF)
setc(USE_ARIA_FOR_TMP_TABLES OFF)
setc(WITH_DBUG_TRACE OFF)
setc(WITH_WSREP OFF)
setc(WITH_NUMA OFF STRING)
setc(WITH_SAFEMALLOC OFF)
setc(WITH_UNIT_TESTS OFF)

set(plugins_exclude
  PLUGIN_CONNECT
  PLUGIN_CRACKLIB_PASSWORD_CHECK
  PLUGIN_OQGRAPH
  PLUGIN_MROONGA
  PLUGIN_TOKUDB
  PLUGIN_SPIDER
  PLUGIN_SPHINX
  PLUGIN_ROCKSDB
  PLUGIN_SEMISYNC_MASTER
  PLUGIN_SEMISYNC_SLAVE
  PLUGIN_AUDIT_NULL
  PLUGIN_AUTH_0X0100
  PLUGIN_AUTH_PAM_V1
  PLUGIN_AUTH_ED25519
  PLUGIN_AUTH_SOCKET
  PLUGIN_AUTH_TEST_PLUGIN
  PLUGIN_BLACKHOLE
  PLUGIN_DAEMON_EXAMPLE
  PLUGIN_DEBUG_KEY_MANAGEMENT
  PLUGIN_DIALOG_EXAMPLES
  PLUGIN_DISKS
  PLUGIN_EXAMPLE
  PLUGIN_EXAMPLE_KEY_MANAGEMENT
  PLUGIN_FEDERATED
  PLUGIN_FEDERATEDX
  PLUGIN_FEEDBACK
  PLUGIN_FILE_KEY_MANAGEMENT
  PLUGIN_FTEXAMPLE
  PLUGIN_HANDLERSOCKET
  PLUGIN_LOCALES
  PLUGIN_METADATA_LOCK_INFO
  PLUGIN_QA_AUTH_CLIENT
  PLUGIN_QA_AUTH_INTERFACE
  PLUGIN_QA_AUTH_SERVER
  PLUGIN_SERVER_AUDIT
  PLUGIN_SIMPLE_PASSWORD_CHECK
  PLUGIN_TEST_SQL_DISCOVERY
  PLUGIN_TEST_VERSIONING
)

foreach(plugin ${plugins_exclude})
  setc(${plugin} NO)
endforeach()

function(convert_dynamic_to_static)
  get_cmake_property(vars VARIABLES)
  list (SORT vars)
  foreach (var ${vars})
    string(REGEX MATCH ^PLUGIN_ m ${var})
    if (m)
      string(REGEX MATCH ^DYNAMIC$ m "${${var}}")
      if (m)
        setc(${var} STATIC STRING)
      endif()
    endif()
  endforeach()
endfunction()

if (DISABLE_SHARED)
  # Make DYNAMIC plugins STATIC because of DISABLE_SHARED
  convert_dynamic_to_static()
else()
  # PAM plugin does not compile as static (see MODULE_ONLY in auth_pam/CMakeLists.txt)
  setc(PLUGIN_AUTH_PAM DYNAMIC STRING)
endif()
