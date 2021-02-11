cmake_minimum_required (VERSION 3.15.2)

project(saxml VERSION 1.0.0)

set(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(SOURCES
    ${BASE_DIR}/saxml.c
    )

set(INCLUDES
    ${BASE_DIR}/saxml.h
    )

add_library(${PROJECT_NAME}
                ${INCLUDES}
                ${SOURCES}
)

target_link_libraries(${PROJECT_NAME}
)

target_include_directories(${PROJECT_NAME} PUBLIC
                            ${BASE_DIR}
)