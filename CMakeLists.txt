 if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(lib-crypto libcrypto-1_1-x64.dll)
  set(lib-ssl libssl-1_1-x64.dll)
 # 32 bit
 elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(lib-crypto libcrypto-1_1-x32.dll)
  set(lib-ssl libssl-1_1-x32.dll)
 endif ()
 
 configure_file(${OPENSSL_ROOT}/bin/${lib-crypto} ${CMAKE_CURRENT_BINARY_DIR}/bin/${lib-crypto} COPYONLY)
 configure_file(${OPENSSL_ROOT}/bin/${lib-ssl} ${CMAKE_CURRENT_BINARY_DIR}/bin/${lib-ssl} COPYONLY)
endif ()
 
if (USE_OPENCL)
 target_link_libraries(creepMiner ${OpenCL_LIBRARY})
 configure_file(${CMAKE_SOURCE_DIR}/src/shabal/opencl/mining.cl ${CMAKE_CURRENT_BINARY_DIR}/bin/mining.cl COPYONLY)
endif ()
 
##################################################################
# Naming
##################################################################
set_target_properties(creepMiner PROPERTIES DEBUG_POSTFIX -d)
 
##################################################################
# Installing
##################################################################
install(TARGETS creepMiner RUNTIME DESTINATION .)
install(FILES src/shabal/opencl/mining.cl DESTINATION .)
install(DIRECTORY bin/public DESTINATION .)
 
if (OVERWRITE_MINING_CONF)
 install(FILES bin/mining.conf DESTINATION .)
endif  ()
 
if (MSVC)
 install(FILES
  ${OPENSSL_ROOT}/bin/${lib-crypto}
  ${OPENSSL_ROOT}/bin/${lib-ssl}
  DESTINATION .)
else ()
 install(FILES
  bin/run.sh
  icon.png
  DESTINATION .)
endif ()
