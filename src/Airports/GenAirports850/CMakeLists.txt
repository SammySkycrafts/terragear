include_directories(${GDAL_INCLUDE_DIR})
include_directories(${GSL_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/src/Lib)
include_directories(${PROJECT_SOURCE_DIR}/src/Lib/terragear)

add_executable(genapts850
    airport.hxx airport.cxx
    apt_math.hxx apt_math.cxx
    beznode.hxx
    closedpoly.hxx closedpoly.cxx
    debug.hxx debug.cxx
    elevations.cxx elevations.hxx
    global.hxx
    helipad.hxx helipad.cxx
    lights.cxx
    linearfeature.hxx linearfeature.cxx
    linked_objects.hxx linked_objects.cxx
    main.cxx
    object.hxx object.cxx
    output.hxx output.cxx
    parser.hxx parser.cxx
    runway.cxx runway.hxx
    rwy_simple.cxx
    rwy_gen.cxx
    scheduler.cxx scheduler.hxx
    taxiway.cxx taxiway.hxx
)

target_link_libraries(genapts850
    terragear
    Array
    ${GDAL_LIBRARY}
    ${ZLIB_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SIMGEAR_CORE_LIBRARIES}
    ${SIMGEAR_CORE_LIBRARY_DEPENDENCIES}
    ${Boost_LIBRARIES}
    ${GSL_LIBRARIES})

install(TARGETS genapts850 RUNTIME DESTINATION bin)
