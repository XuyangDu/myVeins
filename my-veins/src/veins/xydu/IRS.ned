//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package org.car2x.veins.xydu;


import org.car2x.veins.xydu.PhyIRS;
import org.car2x.veins.xydu.OutGate;

module IRS
{
    gates:
        input veinsradioIn; // gate for sendDirect
		
    submodules:

        phy: PhyIRS {
            @display("p=69,218;i=block/process_s");
        }

        out: OutGate {
            @display("p=69,82");
        }
        
    connections:
        
        out.lowerControlOut --> phy.upperControlIn;
        out.lowerLayerOut --> phy.upperLayerIn;
        phy.upperLayerOut --> out.lowerLayerIn;
        phy.upperControlOut --> out.lowerControlIn;

        veinsradioIn --> phy.radioIn;

}