# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-rail cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(FREERDP_RAIL_SRCS
	window_list.c
	window.c
	icon.c
	rail.c
	librail.h)

if(WITH_MONOLITHIC_BUILD)
	add_library(freerdp-rail OBJECT ${FREERDP_RAIL_SRCS})
else()
	add_library(freerdp-rail ${FREERDP_RAIL_SRCS})
endif()

set_target_properties(freerdp-rail PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(FREERDP_RAIL_LIBS)

if(WITH_MONOLITHIC_BUILD)

else()
	set(FREERDP_RAIL_LIBS ${FREERDP_RAIL_LIBS} freerdp-utils)
	target_link_libraries(freerdp-rail ${FREERDP_RAIL_LIBS})
	install(TARGETS freerdp-rail DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
