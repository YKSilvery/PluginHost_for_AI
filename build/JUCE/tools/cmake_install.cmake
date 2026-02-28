# Install script for directory: E:/Codes/PluginHost/PluginiHost_for_AI/JUCE

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/HeadlessPluginHost")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Custom")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/extras/Build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-8.0.12" TYPE FILE FILES
    "E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/JUCEConfigVersion.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/JUCEConfig.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/JUCECheckAtomic.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/JUCEHelperTargets.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/JUCEUtils.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/JuceLV2Defines.h.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/LaunchScreen.storyboard"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/PIPAudioProcessor.cpp.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/PIPAudioProcessorWithARA.cpp.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/PIPComponent.cpp.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/PIPConsole.cpp.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/RecentFilesMenuTemplate.nib"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/UnityPluginGUIScript.cs.in"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/checkBundleSigning.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/copyDir.cmake"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/juce_runtime_arch_detection.cpp"
    "E:/Codes/PluginHost/PluginiHost_for_AI/JUCE/extras/Build/CMake/juce_LinuxSubprocessHelper.cpp"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "E:/Codes/PluginHost/PluginiHost_for_AI/build/JUCE/tools/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
