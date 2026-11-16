# Generates a C++ source that embeds one text file as a string constant.
#
#   activityos_embed_text_file(<input> <output.cpp> <namespace> <function>)
#
# The generated source defines `std::string_view <namespace>::<function>()`.
# Editing the input re-runs the generation.
function(activityos_embed_text_file input output namespace function)
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Cannot embed '${input}': the file does not exist")
    endif()
    file(READ "${input}" text)
    string(LENGTH "${text}" text_length)
    # MSVC rejects string literals longer than about 16 KB.
    if(text_length GREATER 16000)
        message(FATAL_ERROR
            "'${input}' is ${text_length} bytes; keep it under 16000 bytes so it fits "
            "in one string literal on every compiler")
    endif()
    if(text MATCHES "\\)activityos_text\"")
        message(FATAL_ERROR "'${input}' contains the reserved text )activityos_text\"")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")

    get_filename_component(input_name "${input}" NAME)
    file(CONFIGURE OUTPUT "${output}" @ONLY CONTENT
"// Generated from ${input_name}. Do not edit; change that file instead.
#include <string_view>

namespace ${namespace} {

std::string_view ${function}();

std::string_view ${function}() {
    return R\"activityos_text(${text})activityos_text\";
}

} // namespace ${namespace}
")
endfunction()
