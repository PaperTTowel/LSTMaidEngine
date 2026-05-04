function(add_engine_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME}Lib)
    target_compile_definitions(${target} PRIVATE
        TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/test_output"
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_engine_test(JsonTests
    test_json.cpp
)

add_engine_test(SceneSerializerTests
    test_scene_serializer.cpp
)

add_engine_test(MaterialIoTests
    test_material_io.cpp
)

add_engine_test(SpriteMetadataTests
    test_sprite_metadata.cpp
)

add_engine_test(AssetDatabaseTests
    test_asset_database.cpp
)

add_engine_test(SceneSystemTests
    test_scene_system.cpp
)

add_engine_test(TransformTests
    test_transform.cpp
)
