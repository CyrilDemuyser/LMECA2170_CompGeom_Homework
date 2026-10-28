
# ADD HERE THE NAME OF THE FILE (without the .c, 1 per line)
# This will create an ./exec/name executable for each "name" in the list
set(EXEC_LIST
    0_example_viewer
    1_triangulation
    2_ashape_adaptation
    3_1_distance_queries
    3_2_intersection_queries
    4_complete_project
)

foreach(EXEC ${EXEC_LIST})
    add_executable(${EXEC})
    target_sources(${EXEC} PRIVATE "${EXEC}.c")
    target_link_libraries(${EXEC} PUBLIC geonum)
endforeach()
