set(public_headers
    "${PUBLIC_INCLUDE_DIR}/pae/export.h"
    "${PUBLIC_INCLUDE_DIR}/pae/version.h"
    "${PUBLIC_INCLUDE_DIR}/pae/protocol_description.h"
    "${PUBLIC_INCLUDE_DIR}/pae/compiler.h"
    "${PUBLIC_INCLUDE_DIR}/pae/codec.h"
    "${PUBLIC_INCLUDE_DIR}/pae/host_endpoint.h"
    "${PUBLIC_INCLUDE_DIR}/pae/stream_framer.h"
)

foreach(header IN LISTS public_headers)
    file(READ "${header}" content)
    foreach(forbidden IN ITEMS "../" "src/" "config_compiler" "protocol_plan" "ui_description" "PAE_ENABLE_SCHEMA_" "Qt")
        string(FIND "${content}" "${forbidden}" position)
        if(NOT position EQUAL -1)
            message(FATAL_ERROR "Public header ${header} contains forbidden token: ${forbidden}")
        endif()
    endforeach()
endforeach()
