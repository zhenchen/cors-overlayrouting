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

#ifndef __CLIENT_RSLTRFACTORY_H
#define __CLIENT_RSLTRFACTORY_H

#include <vector>
#include <string>
#include "relayselector.h"
#include "simplerlysltr.h"

class CorsSocket;
/** Factory class for producing RelaySelector object
 */
class RsltrFactory {
public:
    /** get the instance of RsltrFactory
     * \return pointer to the only one static instance
     */
    static RsltrFactory * getInstance();
    /** create a RelaySelector object according to param
     * \param corssk pointer to a CorsSocket object
     * \param param name and configuration for construction
     */
    RelaySelector * create_rsltr(CorsSocket * corssk, std::vector<std::string> & param);

private:
    static RsltrFactory * instance;

    RsltrFactory();
};

#endif
