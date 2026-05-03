function(add_engine_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/glm
    )
    target_compile_definitions(${target} PRIVATE
        TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/test_output"
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_engine_test(JsonTests
    test_json.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/json.cpp
)

add_engine_test(SceneSerializerTests
    test_scene_serializer.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/json.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/scene.cpp
)

add_engine_test(MaterialIoTests
    test_material_io.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/json.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/material_io.cpp
)

add_engine_test(SpriteMetadataTests
    test_sprite_metadata.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/json.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/sprite_metadata.cpp
)

add_engine_test(AssetDatabaseTests
    test_asset_database.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/asset_database.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/IO/json.cpp
)
