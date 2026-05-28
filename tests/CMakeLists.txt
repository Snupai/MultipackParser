add_executable(test_settings_database
    test_settings_database.cpp
)
target_link_libraries(test_settings_database
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_settings_database COMMAND $<TARGET_FILE:test_settings_database>)

add_executable(test_xmlrpcserver
    test_xmlrpcserver.cpp
)
target_link_libraries(test_xmlrpcserver
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_xmlrpcserver COMMAND $<TARGET_FILE:test_xmlrpcserver>)

add_executable(test_usbmonitor
    test_usbmonitor.cpp
)
target_link_libraries(test_usbmonitor
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_usbmonitor COMMAND $<TARGET_FILE:test_usbmonitor>)

add_executable(test_mainwindow_integration
    test_mainwindow_integration.cpp
)
target_link_libraries(test_mainwindow_integration
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_mainwindow_integration COMMAND $<TARGET_FILE:test_mainwindow_integration>)

add_executable(test_application_startup
    test_application_startup.cpp
)
target_link_libraries(test_application_startup
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_application_startup COMMAND $<TARGET_FILE:test_application_startup>)

add_executable(test_logging_config
    test_logging_config.cpp
)
target_link_libraries(test_logging_config
    PRIVATE
        multipack-lib
        Qt6::Test
)
add_test(NAME test_logging_config COMMAND $<TARGET_FILE:test_logging_config>)

if(NOT WIN32)
    set(TEST_QT_ENV "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(
        test_settings_database
        test_xmlrpcserver
        test_usbmonitor
        test_mainwindow_integration
        test_application_startup
        test_logging_config
        PROPERTIES
            ENVIRONMENT "${TEST_QT_ENV}"
    )
endif()
