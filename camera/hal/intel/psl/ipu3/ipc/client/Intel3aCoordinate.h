/*
 * Copyright (C) 2017 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PSL_IPU3_IPC_CLIENT_INTEL3ACOORDINATE_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3ACOORDINATE_H_

#include <ia_coordinate.h>
#include "Intel3aCommon.h"

NAMESPACE_DECLARATION {
class Intel3aCoordinate {
public:
    Intel3aCoordinate();
    virtual ~Intel3aCoordinate();

    // if there is error, return: {-1, -1}
    ia_coordinate convert(
                    const ia_coordinate_system& src_system,
                    const ia_coordinate_system& trg_system,
                    const ia_coordinate& src_coordinate);

private:
    Intel3aCommon mCommon;

    bool mInitialized;

    ShmMemInfo mMem;
};
} NAMESPACE_DECLARATION_END
#endif // PSL_IPU3_IPC_CLIENT_INTEL3ACOORDINATE_H_
