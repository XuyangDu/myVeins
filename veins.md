# 接口规律：
AToBInterface
A里FindModule<AToBInterface*>
B里class VEINS_API B : public AToBInterface

# 各层数据和控制消息类型：
## 应用Appl层/网络层：
BaseFrame1609_4
## 链路mac层：
Mac80211Pkt
## 物理phy层：
AirFrame11p

# Appl层：
应用层使用的消息：
BaseFrame1609_4：应用层数据消息，保存有接受者的MAC层地址，-1为广播地址。
DemoServiceAdvertisment：广告信道切换消息

LAddress::L2Type MAC层地址 如果有AddressingInterface由其分配地址，如果没有，使用NIC索引作为MAC层地址

LAddress::L3Type Appl层地址 用节点索引做Appl层地址

TraCIDemo11p，DemoBaseApplLayer的扩展，功能：当车辆节点发生事故停止移动，广播停止堵塞消息。另外有信道控制消息，DemoServiceAdvertisment消息在CCH上发送之后，信道会切换为SCH发送广播消息，不过仿真示例中dataOnSch为false，实际并没有使用SCH。

DemoBaseApplLayer：应用层示例，扩展自BaseApplLayer，BaseApplLayer只留下了接口，没有功能实现，应用层的基本功能基本都在此实现。
## 初始化过程：
DemoBaseApplLayer::initialize()，stage0：首先调用BaseApplLayer::initialize()。然后获取指向其他模块的指针，包括mobility、traci、traciVehicle、mac、annotations，并载入参数。监听BaseMobility::mobilityStateChangedSignal和TraCIMobility::parkingStateChangedSignal信号。最后初始化统计量。

stage1：从mac层获取mac层地址，然后进行Beacon周期自调度，仿真示例中sendBeacons = false。
## 接收到消息的处理：
### 收到下层消息：
判断收到的消息是bsm、wsa还是wsm，修改统计量后分别调用onBSW()，onWSA()，onWSM()。其中仿真示例没有使用Beacons，onBSW()没有实装，onWSA()和onWSM()在TraCIDemo11p和TraCIDemoRSU11p中有实现。
TraCIDemo11p::onWSA() 切换mac层使用的信道，更新currentSubscribedServiceId。
TraCIDemo11p::onWSM() 尝试换道，触发广播道路堵塞信息(TraCIDemoAppl实现事故道路堵塞通知)。
### 收到自消息：
DemoBaseApplLayer的自消息有两种，SEND_BEACON_EVT和SEND_WSA_EVT。

收到SEND_BEACON_EVT，首先调用DemoBaseApplLayer::populateWSM()填充消息，然后把Beacon发送给下层MAC层，并继续调度SEND_BEACON_EVT消息，周期性发送Beacons。

收到SEND_WSA_EVT，触发周期性WSA广播，首先调用DemoBaseApplLayer::populateWSM()填充消息，然后把wsa发送给下层MAC层，并继续调度SEND_WSA_EVT消息，周期性发送WSA。

DemoBaseApplLayer::populateWSM()，首先为wsm设置rcvId和头长，注意rcvId默认为广播地址。然后为不同的数据包配置不同属性。为bsm设置SenderPos、SenderSpeed、Psid = -1、ChannelNumber = cch、UserPriority并添加BitLength。为wsa设置ChannelNumber = cch、TargetChannel、Psid、ServiceDescription。为wsm设置UserPriority和ChannelNumber，若不使用SCH，信道为CCH，否则为SCH，并添加BitLength。

TraCIDemo11p中多出一种自消息wsm，会根据Serial重复广播wsm。
### 监听到信号改变：
BaseMobility::mobilityStateChangedSignal：调用handlePositionUpdate。TraCIDemo11p::handlePositionUpdate()会先调用DemoBaseApplLayer::handlePositionUpdate()，更新Appl层中记录的移动性。然后判断如果停止移动的时间是否达到10s，如果达到10s，判断是发生了事故，会发送wsm。

TraCIMobility::parkingStateChangedSignal：调用DemoBaseApplLayer::handleParkingUpdate()，修改Appl层中记录的isParked。
## wsa的Service：
TraCIDemo11p调用wsa的Service，在DemoBaseApplLayer中准备有DemoBaseApplLayer::startService()和DemoBaseApplLayer::stopService()函数。

# MAC层：
MacToNetwControlInfo：Mac层向应用层的控制信息，成员变量：bitErrorRatte，lastHopMac，rssi
NetwToMacControlInfo：应用层向Mac层的控制信息，成员变量：nextHopMac

BaseMacLayer：提供应用层下发的数据包的封装和来自物理层的数据包的解封，以及收到消息后的处理接口。

Mac1609_4：提供SCH、CCH信道切换、单播、广播处理，useSCH，idelChannel，txPower，mcs

与物理层从上层收到数据就进行发送处理不同，mac层配置有EDCA系统，在从上层收到数据后，会将数据包插入到EDCA队列中，然后通过nextMacEvent处理EDCA队列中的待发送数据包。
## 初始化过程：
BaseMacLayer::initialize() 首先获取到phy的接口，载入参数，然后设置LAddress::L2Type地址，即为Mac层设置地址，如果有AddressingInterface由其分配地址，如果没有，使用NIC索引作为MAC层地址。接下来载入单播所用参数。

Mac1609_4的初始化：首先调用BaseMacLayer::initialize()。首先获取到phy11p的接口，载入参数txPower和bitrate，然后用Mac1609_4::setParametersForBitrate(uint64_t bitrate)，根据bitrate和BANDWIDTH_11P计算所用的mcs。

下一步分别为SCH和CCH创建EDCA系统，在SCH和CCH的EDCA中，为每个Access_Category创建一个EDCAQueue。EDCA创建后，若使用SCH，设置所用的SCH，判断当前活跃信道是SCH还是CCH，并调度Channel切换消息。如果不使用SCH，将活跃的信道设为CCH。最后，初始化统计量，设置信道空闲状态idleChannel = true，调度信道空闲处理函数Mac1609_4::channelIdle(true)，该函数在后文有介绍。
## 收到自消息的处理：
### stopIgnoreChannelStateMsg
把ignoreChannelState = false
### AckTimeOutMessage
调用Mac1609_4::handleAckTimeOut(AckTimeOutMessage* ackTimeOutMsg)。

handleAckTimeOut，处理收到Ack超时提醒后需要重传的情况。首先，判断如果正在RX，则等待期结束，记录lastAC为ackTimeOutMsg->getKind()。然后，禁止RX，调用Mac1609_4::handleRetransmit(t_access_category ac)进行重传，最后调用phy11p->requestChannelStatusIfIdle()更新Channel空闲状态。

handleRetransmit处理重传。首先，将ackTimeOut消息取消掉。取出EDCA中对应ac队列的队头appPkt，将其重传次数+1，判断是否超过重传上限。如果没有超过重传上限，将其waitForAck设为false(表示需要发送)。如果达到重传上限，把该Pkt从EDCA队列弹出，发送sigRetriesExceeded信号，重置重传次数等变量。最后，如果需要重传或弹出重传超限Pkt后队列不空，调用startContent(lastIdle, guardActive())找到下一个mac事件，并进行nextMacEvent自调度。
### nextChannelSwitch
首先自调度下一次信道切换消息。调用Mac1609_4::channelBusySelf(false)，可以理解为切换时信道忙，然后调用Mac1609_4::setActiveChannel(ChannelType state)，将信道进行切换，信道切换后可以调用Mac1609_4::channelIdle(bool afterSwitch)将信道状态恢复为idle，最后通过物理层接口phy11p->changeListeningChannel(Channel::sch/cch)修改物理层监听的信道，物理层会根据监听的信道序号从频段与信道的映射中找出其所对应的信号所在频段。

Mac1609_4::channelBusySelf(bool generateTxOp)，处理信道忙：idleChannel设为false，记录lastBusy时间，取消当前调度中的nextMacEvent，对EDCA系统中的当前活跃信道调用stopContent(false, generateTxOp)，发送信道忙信号。Mac1609_4::EDCA::stopContent(bool allowBackoff, bool generateTxOp) 停止contention，更新回退。

Mac1609_4::setActiveChannel(ChannelType state) 把信道切换到state。

Mac1609_4::channelIdle(bool afterSwitch) 设置idleChannel = true，如果使用SCH，记录lastIdle为GuardOver时间，在EDCA的当前信道中startContent，得到下一个mac事件的时间，并进行nextMacEvent自调度，发送sigChannelBusy信号。如果信道剩余时间不足以完成下一个mac事件，调度Mac1609_4::EDCA::revokeTxOPs()。Mac1609_4::EDCA::startContent(simtime_t idleSince, bool guardActive) 返回信道空闲后EDCA中最近的可能Mac事件时间。Mac1609_4::EDCA::revokeTxOPs() 将txOP == true的edcaQueue的txOP = false，currentBackoff = 0。
### nextMacEvent
收到nextMacEvent自消息，意味着要进行下一次Mac事件，即发送数据。首先，因为要发送数据，调用channelBusySelf(true)将信道设为忙。然后调用initiateTransmit(lastIdle)找到要发送的消息。接下来在lastAC和pktToSend中保留要发送的packet的信息。下一步进行发送，要发送的mac层数据包类型为Mac80211Pkt，从要发送的应用/网络层数据包中获取目标mac地址，将当前mac的地址设为源地址，然后用mac层数据包调用encapsulate将应用层数据包封装其中，准备好要发送的数据包。使用的mcs和txPower信息从应用层数据包的控制信息中得到。发送持续时间调用phy11p->getFrameDuration(mac->getBitLength(), usedMcs)得到，该函数定义在PhyLayer80211p中。最后调用sendFrame发送数据包，如果是单播且使用ack，再额外进行ack处理。如果使用sch且信道剩余时间不足，调用myEDCA[activeChannel]->revokeTxOPs()和channelIdle()取消发送，等待信道切换回来。

Mac1609_4::EDCA::initiateTransmit(simtime_t lastIdle)，返回值为BaseFrame1609_4，该函数遍历EDCA的队列，找到要发送的packet。先找到要发送的队列，队首的消息作为要发送的数据包，如果有多个队列有消息可发送，进行内部争用，低优先级的队列回退。

Mac1609_4::sendFrame(Mac80211Pkt* frame, simtime_t delay, Channel channelNr, MCS mcs, double txPower_mW)，首先调用phy->setRadioState(Radio::TX)将物理层Radio进行切换。@@IRS修改相关 可以考虑在此根据来自应用层的控制信息对波束对准进行控制，这需要对准信息和IRS分配情况在应用层处理好，这里只负责修改物理层配置@@ 接下来配置控制信息，在lastMac中记录发送的frame，最后使用sendDelayed将frame发送到下层，并根据包的类型发送sigSentAck或sigSentPacket信号。
## 收到上层Appl层控制消息的处理：
Mac1609_4中mac层没有对收到Appl层消息的处理。
## 收到上层Appl层数据消息的处理：
收到来自上层的BaseFrame1609_4后，首先根据其优先级找到对应的t_access_category ac和所用的信道，然后调用Mac1609_4::EDCA::queuePacket(t_access_category ac, BaseFrame1609_4* msg)将msg插入EDCA的队列中。如果msg插入了一个空队列且当前信道idle且活跃，直接调用startContent，调度nextMacEvent，如果msg插入了空队列但信道忙，则该队列回退。如果msg插入的不是空队列，正常等待即可，不需要尝试发送或回退。
## 收到下层Phy层控制消息的处理：
### MacToPhyInterface::PHY_RX_START
将rxStartIndication = true
### MacToPhyInterface::PHY_RX_END_WITH_SUCCESS
接收成功，很快会handleLowerMsg，不需要额外操作。
### MacToPhyInterface::PHY_RX_END_WITH_FAILURE
接收失败处理。
### MacToPhyInterface::TX_OVER
发送完成，调用phy->setRadioState(Radio::RX)将物理层的Radio设回RX。调用postTransmit(lastAC, lastWSM, useAcks)更新EDCA队列。

Mac1609_4::EDCA::postTransmit(t_access_category ac, BaseFrame1609_4* wsm, bool useAcks)，如果完成的是单播且使用ack，进行ack配置，如果不是，更新EDCA队列、窗口大小和回退值。
### Mac80211pToPhy11pInterface::CHANNEL_BUSY
信道忙控制信号，调用channelBusy()。

Mac1609_4::channelBusy()，处理信道忙改变，与Mac1609_4::channelBusySelf(bool generateTxOp)类似。
### Mac80211pToPhy11pInterface::CHANNEL_IDLE
当phy完成消息接收后，会用此控制消息通知mac信道空闲，如果是单播且使用ack，此时mac会立即将phy的Radio切换为TX发送Ack，信道就不会空闲，所以如果是单播且使用ack，会有ignoreChannelState变量为true，忽略这个控制消息，否则会调用channelIdle()，进行信道空闲处理。
### Decider80211p::BITERROR、COLLISION、RECWHILESEND，MacToPhyInterface::RADIO_SWITCHING_OVER
记录对应统计量，Decider80211p::COLLISION会发送冲突信号。
### BaseDecider::PACKET_DROPPED
丢包后会把物理层phy的Radio设为RX。
## 收到下层Phy层数据消息的处理：
首先从msg中抽取出DeciderResult80211。若目的地址是当前mac，若是ack，调用handleAck，若是wsm，解封装，设置src地址后配置控制信息，调用handleUnicast。若目的地址是广播地址，调用handleBroadcast。
### Mac1609_4::handleAck(const Mac80211Ack* ack)
收到Ack确认，进行Ack处理，遍历EDCA队列找到Ack对应的msg，重置Ack相关变量，重置窗口大小、回退，若找到对应msg完成了处理，最后把waitUntilAckRXorTimeout = false
### Mac1609_4::handleUnicast(LAddress::L2Type srcAddr, unique_ptr<BaseFrame1609_4> wsm)
收到单播数据包，如果使用Ack，发送Ack。用sendUp上传。

Mac1609_4::sendAck(LAddress::L2Type recpAddress, unsigned long wsmId)，要发送Ack，配置ignoreChannelState = true，调用channelBusySelf(true)，配置Ack的msg信息，注意其中wsmId是Ack要确认的收到的msg，然后求出持续时间，调用sendFrame发送。，并自调度stopIgnoreChannelStateMsg消息。
### Mac1609_4::handleBroadcast(Mac80211Pkt* macPkt, DeciderResult80211* res)
收到广播数据包，解封装msg，配置控制信息，用sendUp上传。

# 物理层：
物理层功能和实现逻辑：

发送过程：收到来自上层mac层的数据，将其封装后通过ChannelAccess的sendToChannel发送给其他NIC，在封装过程中会为其附上signal。

接收过程：收到AirFrame后的处理分为start_receive，receiving，end_receive三步，start步进行天线增益和非损耗型模型的处理，并添加上损耗模型，receiving步用Decider::processSignal进行处理，receiving步进行两次，第一次processSignal用processNewSignal处理，主要是处理损耗模型的影响，判断收到的信号强度是否高于最低阈值，即接收者能否成功收到。第二次processSignal用processSignalEnd处理，先求出sinr和snr，求出数据率，最后求出错误率，判断包是否能成功解码，如果成功则sendUp。end_receive步删除channelInfo中传输过程中保存的AirFrame。

AirFrame、channel、siganl三者关系：AirFrame是在物理层发送的数据，其需要在某个channel中、以某个signal发送。channel是信道，比如控制信道、服务信道，80211p的服务信道有多个，signal是所用的信号，有信号强度、所用的衰减模型、载波频段等信息，channel和siganl有依赖关系，siganl所用的载波频段，由所用的channel决定，即每个channel被分配有不同的频段，80211p的信道与其对应的载波在Consts80211p.h中定义，IEEE80211ChannelFrequencies。

物理层涉及到的模块关系：
ChannelAccess：负责物理层使用的channel处理，物理层的数据发送最终会在ChannelAccess中利用ConnectionManager完成。负责监听其所在物理层对应节点的移动性改变，若接收到移动性改变信号，调用cc->updateNicPos。
需要注意的成员变量：BaseConnectionManager* cc，antennaPosition，antennaHeading

BaseDecider：处理信号的接收判断和channel的Idle与否，并在成功接收后将数据sendUp。核心函数processSignal()

Decider80211p：继承自BaseDecider，80211p的decider，重定义了processNewSignal和processSignalEnd，在BaseDecider的基础上，处理信号的衰减，求解接收信号的信噪比、误码率，并依次做出能否成功接收的判断。cca(simTime(), nullptr)用于判断信道在噪声和干扰的影响下是否能够保持空闲。

BasePhyLayer：物理层模板。
类的关系：继承自ChannelAccess，提供信道支持，DeciderToPhyInterface，提供物理层到decider的接口，MacToPhyInterface，提供物理层到mac层的接口。
需要注意的成员变量：channelInfo，radio，antenna，decider，analogueModels，analogueModelsThresholding，overallSpectrum。

PhyLayer80211p：80211p的物理层，实装了损耗模型定义，重定义了部分函数，主要是和信号损耗与传输判断相关的函数。
类的关系：继承自BasePhyLayer，Mac80211pToPhy11pInterface，Decider80211pToPhy80211pInterface。
需要注意的成员变量：与BasePhyLayer基本一致。
## 初始化过程：
PhyLayer80211p中载入参数，设置载波，然后调用BasePhyLayer::initialize,注意BasePhyLayer继承自ChannelAccess，会调用ChannelAccess::initialize。

BasePhyLayer::initialize：载入参数，initializeRadio，initializeAnalogueModels，initializeDecider，initializeAntenna。

initializeAnalogueModels，initializeDecider，initializeAntenna都是在先通过BasePhyLayer::getParametersFromXML读取到XML文件中的参数，然后用BasePhyLayer::getxxFromName创建模型，getxxFromName会根据读取到的参数调用不同的模型的initialize来创建模型，其中，getAntennaFromName在BasePhyLayer中已定义，而getDeciderFromName和getAnalogueModelFromName在BasePhyLayer中为虚，在PhyLayer80211p中定义。

initializeAnalogueModels会完成analogueModelsThresholding和analogueModels前者保存损耗模型，即只会降低信号强度的模型，后者保存非损耗型模型，即可能增强或降低信号强度的模型。
## 处理收到的消息：
handleMessage(cMessage* msg)在BasePhyLayer中定义，在PhyLaye80211p中没有重定义。
### 处理自调度消息：
handleselfMessage(cMessage* msg)在BasePhyLayer中已经定义，在PhyLayer80211p中有重定义。

自消息有三类，TX_OVER，RADIO_SWITCHING_OVER和AIR_FRAME。TX_OVER指示传输结束，物理层向Mac层发送传输结束TX_OVER消息，然后利用decider判断TX完成后信道Channel是否Idle。

RADIO_SWITCHING_OVER和AIR_FRAME的处理与BasePhyLayer中保持一致，radio切换结束就调用BasePhyLayer::finishRadioSwitching()，而AIR_FRAME类型的自消息是经decider处理的AirFrame，调用BasePhyLayer::handleAirFrame，继续处理接收的过程。
### 收到来自MAC层的msg：
收到来自上层的控制msg报错，MAC层对Phy的控制直接使用控制接口完成，不发送控制msg。

收到来自上层的数据msg，指示Phy需要将来自上层的数据封装为AirFrame发送到其他Phy，调用BasePhyLayer::handleUpperMessage完成，详情在处理发送的过程中。
## radio的切换：
radio的切换由setRadioState(int rs)函数完成，该函数定义在BasePhyLayer中，在PhyLayer80211p中有重定义，重定义时加上了如果radio切换为TX，将decider也切换为TX。setRadioState(int rs)首先判断是否处在发送中，没有在发送才能切换radio。用radio->switchTo(rs, simTime())开始调度，返回值为调度所需时间，如果时间为0，直接调用finishRadioSwitching()，否则发送自调度消息，在切换结束时调用finishRadioSwitching()。finishRadioSwitching()用radio->endSwitch(simTime())完成切换并向Mac层发送radio切换完成的控制信息。
## 处理发送的过程：
从收到来自mac层的包开始到将AirFrame发送出去结束，由BasePhyLayer::handleUpperMessage函数完成，该函数在PhyLayer80211p中没有重定义。
### 预处理：
从收到来自mac层的包macPkt开始。首先检查radio的状态是否为TX[注：mac层会在向Phy发送macPkt之前将phy->radio的状态改为TX]。然后检查cMessage的txOverTimer是否已被调度，已被调度说明正在发送。如果radio为TX且不在发送，可以发送AirFrame，接下来调用encapsMsg进行AirFrame的封装。
### AirFrame的封装：
AirFrame的封装过程由encapsMsg函数完成，该函数定义在BasePhyLayer中，被PhyLayer80211p继承，但没有重定义。

首先从macPkt取出ControlInfo，用macPkt的name创建一个AirFrame[注：createAirFrame在BasePhyLayer中有定义，但是在PhyLayer80211p中会重定义]，设置AirFrame的优先级，协议，Id，信道，BitLength等。然后调用AirFrame->encapsulate(macPkt)将macPkt封装在创建的AirFrame中。接下来调用attachSignal(frame.get(), ctrlInfo.get())为AirFrame捆绑Signal和控制信息[注：该函数在BasePhyLayer中声明为虚函数，在PhyLayer80211p中重定义]。注意在完成这些后要macPkt = nullptr，因为封装后不能有指向被封装msg的指针，被封装的msg属于封装它的msg，在这里封装后macPkt归属于对应的AirFrame。

attachSignal的处理，首先检查ctrlInfo是否为ctrlInfo11p，MacToPhyControlInfo11p的变量为channelNr，mcs和txPower_mW。然后根据AirFrame封装的数据长度和所用的mcs调用getFrameDuration(int payloadLengthBits, MCS mcs)求得duration[注：该函数是在Mac80211pToPhy11pInterface中声明为虚函数，在PhyLayer80211p中重定义]。然后根据物理层所用频谱、当前仿真时间和AirFrame的持续时间，创建signal，并选择signal所用频段，设置对应频段的功率，频段信息由ctrlInfo11p->channelNr所在的频段决定。最后配置AirFrame的signal、Duration和mcs。
### 后处理：
封装完毕后，设置AirFrame的Poa，即发送者antenna的pos, orient, antenna，其中存储pos和orient的变量在ChannelAccess中定义，被BasePhyLayer继承，antenna在BasePhyLayer中定义。

AirFrame处理完毕后，Phy模块自调度“发送结束”事件，即txOverTimer消息，到达时间为AirFrame结束时间。然后用BasePhyLayer的sendMessageDown将AirFrame发送出去。sendMessageDown调用继承自ChannelAccess的sendToChannel，从ConnectionManager得到与该物理层的nic相连的gate，将AirFrame加上传播时延后发送到那些gate。
## 处理接收的过程：
从收到AirFrame开始，收到AirFrame包会调用handleAirFrame(AirFrame* frame)函数，该函数处理AirFrame的接收，定义在BasePhyLayer中，在PhyLayer80211p中没有重定义。根据收到的AirFrame的state分start_receive、receiving、end_receive处理，从其他模块收到的AirFrame初始state为start_receive。
### 开始接收start_receive：
AirFrame的state为start_receive，则handleAirFrame会调用handleAirFrameStartReceive(AirFrame* frame)，定义在BasePhyLayer中，在PhyLayer80211p中没有重定义。该函数主要进行filterSignal，对signal的强度进行处理，包括接收方和发送方天线增益、非损耗型模型影响。注意：NicEntry和ChannelAccess中的antennaHeading来自mobility->getCurrentOrientation()是天线方向。

首先将AirFrame录入channelInfo，该AirFrame目前尚不知是会被接收还是视为干扰，后边如果该frame的信号经衰减后大于阈值且接收方不在接收，会被视为接收目标，否则会成为干扰，但不管是作为接收目标还是干扰，在后边都会经过增益和衰减，在求sinr时用上。

然后对信号进行处理，调用filterSignal(AirFrame* frame),该函数在BasePhyLayer中定义，在PhyLayer80211p中没有重定义。filterSignal函数首先为AirFrame所用的信号signal设置发送方和接收方的POA，完成天线增益的影响，然后为把物理层的损耗模型加入signal，并调用各非损耗型模型的filterSignal完成非损耗型模型的影响处理。

如果不使用decider和协议，跳过receiving过程，将AirFrame的state更改为end_receive，直接自调度，时间设置为signal的结束时间。否则，接下来进行receiving过程，将AirFrame的state更改为receiving，由于期间没有时间间隔，直接调用handleAirFrameReceiving。
### 接收中receiving：
表面上，Phy会在handleAirFrame中发现AirFrame的state为receiving后调用handleAirFrameReceiving，实际上state为Receiving的自调度不会发生，handleAirFrameReceiving会在start_receive处理完后直接调用。该函数主要进行工作于该物理层上的decider的处理，会调用两次。

在两次调用中都会调用decider->processSignal(AirFrame* s)，返回值为nextHandleTime，即下一次进行AirFrame调度的时间。根据decider的signalState的状态值选择处理方式。首次调度processSignal的没有对应的siganlState会返回NEW，调用的处理方式为processNewSignal。而第二次就会变成processSignalEnd。

processNewSignal在BaseDecider中有定义，在Decider80211p中有重定义，该函数会在signal上处理所有损耗模型，判断损耗后的信号是否高于最低阈值，即接收者能否收到。

processNewSignal函数首先会注册signalState为EXPECT_END，即processNewSignal只会进行一次。然后使用signal.smallerAtCenterFrequency(minPowerLevel)判断信号经过损耗模型衰减后还是否大于最小接收功率，signal.smallerAtCenterFrequency会将signal.analogueModelList中的损耗模型都处理且仅处理一遍。如果衰减后的信号小于最小接收功率，将frame->underMinPowerLevel设为true，接收失败，处理ChannelIdle，返回signal的结束时间。如果衰减后的信号大于最小接收功率，说明可以接收，开始接收，ChannelIdle变为false，然后判断radio是否为TX，如果radio为TX，接收失败，如果radio不是TX(mac层收到phy发送结束的消息后会自动把radio设为RX)，接下来判断是否已经在接收一个signal，如果没有正在接收的frame，会将currentSignal设为当前frame，以区分目标信号和干扰信号，并根据notifyRxStart决定是否通知mac层PHY_RX_START，返回值同样为signal的结束时间。

首次调用decider->processSignal返回值是signal的结束时间，赋给nextHandleTime，作为下一次handleAirFrame的调度时间。下一次调度handleAirFrame时，由于AirFrame的state仍为receiving，仍会调用handleAirFrameReceiving，这是第二次调用handleAirFrameReceiving，仍会调用decider->processSignal，不过第二次调用decider->processSignal时signalState已经注册为EXPECT_END，因此第二次会调用processSignalEnd处理。

processSignalEnd函数同样在BasePhyLayer中定义过，并在PhyLayer80211p中进行了重定义，该函数会先计算出sinr和snr，确定传输数据率，然后利用NistErrorRate模块计算出错误率，判断数据包能否成功解码。

processSignalEnd函数函数首先会将signalState删去，然后依次判断功率是否合格，是否边收边发，是否是目标frame，经过这些判断后，最后调用checkIfSignalOk对frame进行最后判断，经过这些判断后根据判断结果配置deciderresult，如果成功传输，phy->sendUp(frame, result)，并会将成功或失败原因的控制消息发送给mac。

chekIfSignalOk函数，首先计算出sinr和snr，sinr通过ChannelInfo记录下的传输时间内该NIC收到的其他AirFrame信息和噪音计算，snr根据噪音计算。求出sinr和snr后，调用getOfdmDatarate求出payloadBitrate，然后调用packetOk(double sinrMin, double snrMin, int lengthMPDU, double bitrate)得到传输结果。packetOk会利用NistErrorRate模块，根据sinr、snr、数据长度、比特率、带宽确定传输成功率，判断传输是否成功。其中所用mcs与比特率和带宽决定。

processSignalEnd在判断完传输是否成功并完成处理后会返回notAgain，标识着传输判断已经完成，不会再调度handleAirFrame。接下来继续处理handleAirFrameReceiving，这是该函数的第二次调用，也是一次传输中的最后一次调用，此时传输已经完成，仿真时间应该已经等于signal的结束时间，会将AirFrame的State设为end_receive，然后调用handleAirFrameEndReceive。
### 接收完成end_receive：
调用handleAirFrameEndReceive，所做的处理是将channelInfo清空，将channelInfo中当前仿真时间前的AirFrame都清空。

# ConnectionManager：
connection的功能与实现逻辑：
实现连接的创建和维护。在节点的移动性状态改变时，ChannelAccess会监听到移动状态改变信号，调用ConnectionManager中的函数，将移动状态的改变告知ConnectionManager，ConnectionManager会对该节点的NIC进行注册或更新，注册和更新最终都会调用函数完成connection的更新。
相关模块：
ChannelAccess：保存有ConnectionManager的指针，可以在节点移动性发生改变时调度连接的更新。
BaseConnectionManager：基本完成了CM的相关功能。
ConnectionManager：加入了maxInterferenceDistance的参数载入功能。
NicEntry：提供网卡支持。

遗留问题：如果一个connection在AirFrame的传输过程中disconnect，如何处理；connect过程对gate的处理，gatefrom->gateto在哪里实现
## 初始化过程：
BaseConnectionManager中进行初始化，首先载入参数，获取指向world的指针。

然后设置maxInterferenceDistance和maxDistSquared，准备初始化节点网格，分为三步。

第一步，根据playgroundSize和maxInterferenceDistance求出GridCoord的规模。

第二步，初始化nicGrid，用于保存已经register到ConnectionManager的各个网格的NIC。

第三步，准备好节点位置到网格的映射关系，具体方案是求出findDistance，这个变量可以理解为网格的大小。
## NIC的register：
### registerNic：
BaseConnectionManager::registerNic(cModule* nic, ChannelAccess* chAccess, Coord nicPos, Heading heading)函数实现NIC向ConnectionManager的register。

具体操作是创建一个nicEntry，根据nic和信道的信息对其进行配置，然后将配置好的nicEntry加入到ConnectionManager保存的nic中，最后调用BaseConnectionManager::updateConnections(int nicID, Coord oldPos, Coord newPos)在加入新的nic后进行连接检查。

将配置完成后的nicEntry加入到ConnectionManager中分两步，第一步将nicEntry加入到保存了ConnectionManager中所有nic的nics中，第二步调用BaseConnectionManager::registerNicExt(int nicID)将其加入到对应网格的NicEntries中。registerNicExt首先用BaseConnectionManager::getCellForCoordinate(const Coord& c)根据pos获得对应的网格，然后用BaseConnectionManager::getCellEntries(BaseConnectionManager::GridCoord& cell)获得该网格的NicEntries，最后将准备好的nicEntry加入其中。

连接检查函数updateConnections详情在“移动状态更新对connection的影响”中。
### unregisterNic：
BaseConnectionManager::unregisterNic(cModule* nicModule)函数断开NIC到ConnectionManager的register。首先，找到该nic所在的网格，遍历该网格及其周边网格中的nic，如果存在与该nic连接的nic，断开它们的连接。然后从该nic所在的网格的NicEntries中和ConnectionManager保存的nics中删去该nic。
## 移动状态更新对connection的影响：
当节点发生移动状态更新时，该信号会被ChannelAccess::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details)监听到，如果该节点的nic未被register到ConnectionManager，调用BaseConnectionManager::registerNic(cModule* nic, ChannelAccess* chAccess, Coord nicPos, Heading heading)将其register到ConnectionManager；如果该节点的nic已被register到ConnectionManager，调用BaseConnectionManager::updateNicPos(int nicID, Coord newPos, Heading heading)，更新该Nic的移动性信息，这两个函数在分别完成注册和更新工作后，都会调用BaseConnectionManager::updateConnections(int nicID, Coord oldPos, Coord newPos)。

updateConnections会用getCellForCoordinate找到新旧位置对应的网格，调用BaseConnectionManager::checkGrid(GridCoord& oldCell, GridCoord& newCell, int id)对网格进行检查。

checkGrid函数利用BaseConnectionManager::fillUnionWithNeighbors(CoordSet& gridUnion, GridCoord cell)将目标网格周围的网格汇总为一个集合，遍历集合中所有网格中所有的nic，更新它们与目标nic的连接，连接的更新调用函数BaseConnectionManager::updateNicConnections(NicEntries& nmap, nic)完成。

updateNicConnections函数更新一个NicEntries中的所有nic与一个目标nic之间的连接关系，连接的判断由BaseConnectionManager::isInRange(pFromNic, pToNic)完成。
## NicEntry网卡的功能实现：
NicEntry在ConnectionManager中用于存储一个nic的必须信息，提供connectTo和disconnectFrom函数用于建立和删除连接，提供getGateList函数用于返回连接的用于物理层传输的gate。

# mobility：
目前的TraCIMobility处理accident事件，考虑将accidentCount设为0，应该可以直接作为正常的车辆移动性模型使用。
ScenarioManager比较复杂，最好仔细研究后确定是否将accidentCount设为0直接使用TraCIMobility。

move保存移动性，作为mobility模块的成员变量。

BaseMobility实现了移动性节点出界处理，拥有指向ConnectionManager和其host的指针，定义有移动状态改变信号。留出了makemove()和周期更新移动性的函数与接口，但在TraCIMobility里没有使用，移动性的周期更新在ScenarioManager中实现。

ScenarioManager负责移动节点的控制，包括载入XML文件，添加动态移动节点，周期更新移动性，删除动态移动节点等。
## 初始化过程：
BaseMobility::initialize()，stage0载入参数，stage1，处理仿真界面上的显示，调用updatePosition，自调度周期移动。
TraCIMobility::initialize()首先初始化BaseMobility，然后载入TraCIMobility参数。
## 出界处理：
出界处理的相关函数在BaseMobility中完成，不需要太关注。
## 移动性处理：
TraCIMobility::nextPosition(),记录参数，调用TraCIMobility::changePosition()，记录统计量，调用fixIfHostGetsOutside()进行出界判断处理，调用updatePosition(),发送移动性改变信号，处理仿真界面。

# ScenarioManager:
## 初始化过程：
TraCIScenarioManager::initialize()，首先载入参数，包括trafficLightModule相关参数，connectAt，firstStepAt，updateInterval，penetrationRate，ignoreGuiCommands，host，autoShutdown等，调用parseModuleTypes()处理车辆类型与模型的对应，调用getPortNumber()获取端口号，初始化roi，并初始化变量，获取annotations，world，vehicleObstacleControl。调度connectAt和firstStepAt自消息。
## 收到消息的处理(只会收到自消息)
TraCIScenarioManager::handleMessage()只会收到自消息，调用handleSelfMsg()。收到的消息分为connectAndStartTrigger和executeOneTimestepTrigger。
### 收到connectAndStartTrigger：
处理connect。使用TraCIConnection::connect(this, host.c_str(), port)创建ScenarioManager到Traci server的connect。使用TraCICommandInterface(this, *connection, ignoreGuiCommands)创建commandIfc。最后调用init_traci()。

TraCIScenarioManager::init_traci()：获取apiVersion。查询和设置路网边界。订阅出发和到达车辆列表以及模拟时间。订阅车辆序列列表。如果有trafficLight进行初始化。添加障碍物。最后发送traciInitializedSignal信号，绘制并计算 rois 的面积。

processSubcriptionResult(buf)处理订阅。
processVehicleSubscription()，processSimSubscription()，processTrafficLightSubscription()
processVehicleSubscription()，处理车辆订阅相关操作，如果没有对应的车辆模块，创建车辆，如果存在对应车辆模块，进行位置更新。processSimSubscription()，处理车辆和仿真订阅相关操作。processTrafficLightSubscription()处理TL订阅相关操作。

TraCIScenarioManager::addModule()创建节点模块。调用TraCIMobility::changePosition()。
### 收到executeOneTimestepTrigger：
调用executeOneTimestep()。调用processSubcriptionResult()。

# 总结：移动地产生和更新以及对连接的影响过程，关联模块，ScenarioManager，TraCIConnection，Mobility，ConnectionManager，ChannelAccess，Phy
## 车辆的创建和改变中移动性改变信号的发送
### 车辆的添加过程：
TraCIScenarioManager初始化的最后会调度两个自消息，connectAndStartTrigger和executeOneTimestepTrigger。connectAndStartTrigger会建立TraCIConnection::connect，并进行traci的初始化。traci的初始化过程中，会利用TraCIConnection::query()获取订阅信息，并调用TraCIScenarioManager::processSubcriptionResult()处理订阅信息，订阅信息的处理函数有三个分支，processVehicleSubscription()，processSimSubscription()，processTrafficLightSubscription()，分别处理车辆，仿真变化和TL的订阅。

traci的初始化中，processSubcriptionResult()会调用三次，依次处理sim，vehicle，和TL的订阅信息。在处理vehicle的订阅信息时，如果对应车辆不存在(实际上在初始化时肯定不存在)，会调用TraCIScenarioManager::addModule()创建车辆节点，addModule()中创建车辆，初始化其移动性后会调用TraCIMobility::changePosition()，该函数会调用BaseMobility::updatePosition()，其中会发送mobilityStateChangedSignal。
### 车辆添加后的定期移动性改变更新
TraCIScenarioManager调度的另一个自消息是周期性的，executeOneTimestepTrigger自消息会每隔updateInterval调度一次，其触发的事件会向TraCIConnection::connect咨询订阅信息，然后调用TraCIScenarioManager::processSubcriptionResult()依次处理订阅信息，注意这里咨询到的信息数量和类型不明，其中如果有vehicle的订阅信息，会调用processVehicleSubscription()处理，其中如果车辆已经存在，会调用TraCIScenarioManager::updateModulePosition()改变位置信息，该函数会调用TraCIMobility::nextPosition()，其中会调用TraCIMobility::changePosition()，然后会调用BaseMobility::updatePosition()，其中会发送mobilityStateChangedSignal。
## 监听车辆移动性改变信号的模块及其反应
ChannelAccess会监听移动性改变信号，并调用ChannelAccess::receiveSignal()处理，该函数会调用BaseConnectionManager::registerNic()或BaseConnectionManager::updateNicPos()将nic在ConnectionManager中进行注册或更新，注册和更新的最后都会调用BaseConnectionManager::updateConnections()，对连接进行更新。










# 调整方向性天线可能需要用到：
ChannelAccess会监听移动性改变信号，并调用ChannelAccess::receiveSignal()处理，其中会获取天线的位置和方向信息用于Nic在ConnectionManager中的注册和更新，注册和更新后会调整connection。天线方向的获取是mobility->getCurrentOrientation()，该函数会获取mobility模型中的move类中的orientation。

要修改天线方向：需要修改mobility中的move的orientation，我们很难获取move，但是可以获取mobility，在mobility中加入一个setOrientation的接口(原本只有get的接口)，修改orientation方向后发送移动状态改变信号(在mobility中实现)。需要注意移动状态周期性更新时会影响Orientation，可以考虑将TraCIMobility中的move.setOrientationByVector(heading.toCoord())删去，因为TraCIMobility把heading和orientation看做一样，会把orientation跟heading设为一样。


目前修改的部分：MyVeinsApp, BaseMobility, BaseConnectionManager, Mac1609_4, BasePhyLayer, Decider80211p

注意：物理层接收问题，原本是：
如果收到的信号强度低于阈值，不作为接收信号，不改变currentSignal，duration结束后也不向MAC发送消息，当做没有感知到。
收到的信号强度高于阈值，判断当前是否在发送，如果不是在发送，再判断是否在接收，如果没有接收，作为接收信号(占坑)，duration结束后会进行误码判断。如果正在发送或者正在接收，duration后都会直接做失败处理，会向MAC发送失败消息。

修改后，为了让TX与RX匹配，在Appl层直接配置发送方和接收方的RadioState，在MAC层的txOver事件中恢复发送方的RadioState，为了避免接收方收到的信号低于阈值时感知不到错把其他信号当成接收信号，哪怕信号强度低于阈值，也会作为接收信号，即接收方仅会将RadioState被设为RX后收到的第一个AirFrame作为要接收的AirFrame，这个AirFrame极大概率是来自发送方的。即接收方只会处理来自发送方的AirFrame，其他的AirFrame被作为干扰，这样可以保障TX和RX的匹配，哪怕信号很弱，RX时间内也可以占坑。而RX的恢复在接收成功或失败后处理。


创建一个新的模块IRS，更改其他节点的Phy层中的sendToChannel函数，当发送的msg是使用irs发送时，发送节点在发送时仅将msg发送到对应的irs的gate。为IRS模块建立独立的物理层，phyLayerIRS，该物理层无需RadioState，收到AirFrame时，对其信号进行发送方增益处理和衰落处理，然后发送给其他节点。更改其他节点的Phy层中的AirFrame接收，当收到的msg是irs发送时，处理接收方信号增益和irs增益与衰落。irs的增益需要对应于该msg的irs方向，考虑将irs使用信息和irs方向这些附加到控制信息中。