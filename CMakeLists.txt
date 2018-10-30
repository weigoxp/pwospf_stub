cmake_minimum_required(VERSION 3.12)
project(stub_sr C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(stub_sr
        sha1.c
        sha1.h
        sr_dumper.c
        sr_dumper.h
        sr_if.c
        sr_if.h
        sr_main.c
        sr_protocol.h
        sr_router.c
        sr_router.h
        sr_rt.c
        sr_rt.h
        sr_vns_comm.c
        vnlconn.c
        vnlconn.h
        vnscommand.h)
