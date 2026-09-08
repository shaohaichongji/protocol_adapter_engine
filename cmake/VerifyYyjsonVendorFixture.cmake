if(NOT DEFINED PAE_YYJSON_VERIFY_MODULE OR
   NOT DEFINED PAE_YYJSON_LOCK OR
   NOT DEFINED PAE_YYJSON_ROOT)
    message(FATAL_ERROR "yyjson fixture verification arguments are required")
endif()

include("${PAE_YYJSON_VERIFY_MODULE}")
pae_verify_yyjson_vendor("${PAE_YYJSON_LOCK}" "${PAE_YYJSON_ROOT}")
