# Generates a C++ source that embeds every resources/migrations/NNN_name.sql file,
# so the SQL files are the only definition of the database schema.
#
#   activityos_embed_migrations(<directory> <output.cpp>)
#
# The numeric filename prefix is the migration version. Files are embedded in
# version order, and adding or editing a file re-runs the generation.
function(activityos_embed_migrations directory output)
    file(GLOB migration_files CONFIGURE_DEPENDS "${directory}/*.sql")
    list(SORT migration_files)
    if(NOT migration_files)
        message(FATAL_ERROR "No migration files found in ${directory}")
    endif()

    set(entries "")
    set(seen_versions "")
    foreach(migration_file IN LISTS migration_files)
        get_filename_component(name "${migration_file}" NAME_WE)
        if(NOT name MATCHES "^([0-9]+)_")
            message(FATAL_ERROR
                "Migration '${migration_file}' must be named NNN_description.sql")
        endif()
        string(REGEX REPLACE "^0+" "" version "${CMAKE_MATCH_1}")
        if(version STREQUAL "")
            message(FATAL_ERROR "Migration '${migration_file}' must have a version above 0")
        endif()
        if(version IN_LIST seen_versions)
            message(FATAL_ERROR "Duplicate migration version ${version}: ${migration_file}")
        endif()
        list(APPEND seen_versions "${version}")

        file(READ "${migration_file}" sql)
        string(LENGTH "${sql}" sql_length)
        # MSVC rejects string literals longer than about 16 KB.
        if(sql_length GREATER 16000)
            message(FATAL_ERROR
                "Migration '${migration_file}' is ${sql_length} bytes; keep each file "
                "under 16000 bytes so it fits in one string literal on every compiler")
        endif()
        if(sql MATCHES "\\)activityos_sql\"")
            message(FATAL_ERROR
                "Migration '${migration_file}' contains the reserved text )activityos_sql\"")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${migration_file}")
        string(APPEND entries
            "        {${version}, \"${name}\", R\"activityos_sql(${sql})activityos_sql\"},\n")
    endforeach()

    file(CONFIGURE OUTPUT "${output}" @ONLY CONTENT
"// Generated from resources/migrations/*.sql. Do not edit; change the SQL files.
#include \"activityos/storage.hpp\"

namespace activityos::storage {

std::span<const Migration> builtInMigrations() {
    static constexpr Migration kMigrations[] = {
${entries}    };
    return kMigrations;
}

} // namespace activityos::storage
")
endfunction()
