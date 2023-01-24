FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(SRCS ${app_sources})

spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
