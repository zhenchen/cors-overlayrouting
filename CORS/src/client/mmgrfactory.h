// Copyright (C) 2007 Li Tang <tangli99@gmail.com>
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//  
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#ifndef __CLIENT_MMGRFACTORY_H
#define __CLIENT_MMGRFACTORY_H

#include <vector>
#include <string>
#include "multipathmanager.h"
#include "simplempathmanager.h"
#include "statmpathmanager.h"

class CorsSocket;
/** Factory class for producing MultipathManager object
 */
class MmgrFactory {
public:
    /** get the instance of MmgrFactory
     * \return pointer to the only one static instance
     */
    static MmgrFactory * getInstance();
    /** create a MultipathManager object according to param
     * \param corssk pointer to a CorsSocket object
     * \param param name and configuration for construction
     */
    MultipathManager * create_mmgr(CorsSocket * corssk, std::vector<std::string> & param);

private:
    static MmgrFactory * instance;

    MmgrFactory();
};

#endif
