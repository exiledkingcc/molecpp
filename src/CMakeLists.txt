add_executable(mole
    main.cpp
    mole_crypto.cpp
    local_session.cpp
    remote_session.cpp
    utils.cpp
)
target_link_libraries(mole -lpthread -lsodium)