/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#include "gdcmULConnectionManager.h"

#include "gdcmUserInformation.h"
#include "gdcmULEvent.h"
#include "gdcmPDUFactory.h"
#include "gdcmReader.h"
#include "gdcmAAssociateRQPDU.h"
#include "gdcmAttribute.h"
#include "gdcmBaseRootQuery.h"

#include "gdcmAReleaseRPPDU.h"

#include <vector>
#include <socket++/echo.h>//for setting up the local socket

using namespace gdcm::network;


ULConnectionManager::ULConnectionManager(){
  mConnection = NULL;
  mSecondaryConnection = NULL;
}

ULConnectionManager::~ULConnectionManager(){
  if (mConnection != NULL){
    delete mConnection;
    mConnection = NULL;
  }
  if (mSecondaryConnection != NULL){
    delete mSecondaryConnection;
    mSecondaryConnection = NULL;
  }
}

bool ULConnectionManager::EstablishConnection(const std::string& inAETitle,  const std::string& inConnectAETitle,
                                              const std::string& inComputerName, const long& inIPAddress,
                                              const unsigned short& inConnectPort, const double& inTimeout,
                                              const EConnectionType& inConnectionType, const gdcm::DataSet& inDS)
{

  //generate a ULConnectionInfo object
  UserInformation userInfo;
  ULConnectionInfo connectInfo;
  if (inConnectAETitle.size() > 16) return false;//too long an AETitle, probably need better failure message
  if (inAETitle.size() > 16) return false; //as above
  if (!connectInfo.Initialize(userInfo, inConnectAETitle.c_str(),
    inAETitle.c_str(), inIPAddress, inConnectPort, inComputerName)){
    return false;
  }

  if (mConnection!= NULL){
    delete mConnection;
  }
  mConnection = new ULConnection(connectInfo);

  mConnection->GetTimer().SetTimeout(inTimeout);


  // Warning PresentationContextID is important
  // this is a sort of uniq key used by the recevier. Eg.
  // if one push_pack
  //  (1, Secondary)
  //  (1, Verification)
  // Then the last one is prefered (DCMTK 3.5.5)

  // The following only works for C-STORE / C-ECHO
  // however it does not make much sense to add a lot of abstract syntax
  // when doing only C-ECHO.
  // FIXME is there a way to know here if we are in C-ECHO ?
  //there is now!
  //the presentation context will now be part of the connection, so that this
  //initialization for the association-rq will use parameters from the connection

  gdcm::network::AbstractSyntax as;

  std::vector<PresentationContext> pcVector;
  PresentationContext pc;
  gdcm::network::TransferSyntax_ ts;
  ts.SetNameFromUID( gdcm::UIDs::ImplicitVRLittleEndianDefaultTransferSyntaxforDICOM );
  pc.AddTransferSyntax( ts );
  ts.SetNameFromUID( gdcm::UIDs::ExplicitVRLittleEndian );
  //pc.AddTransferSyntax( ts ); // we do not support explicit (mm)
  switch (inConnectionType){
    case eEcho:
        pc.SetPresentationContextID( eVerificationSOPClass );
        as.SetNameFromUID( gdcm::UIDs::VerificationSOPClass );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
      break;
    case eFind:
        pc.SetPresentationContextID( ePatientRootQueryRetrieveInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::PatientRootQueryRetrieveInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID(eStudyRootQueryRetrieveInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::StudyRootQueryRetrieveInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID( ePatientStudyOnlyQueryRetrieveInformationModelFINDRetired );
        as.SetNameFromUID( gdcm::UIDs::PatientStudyOnlyQueryRetrieveInformationModelFINDRetired );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID( eModalityWorklistInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::ModalityWorklistInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID( eGeneralPurposeWorklistInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::GeneralPurposeWorklistInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
      break;
      //our spec does not require C-GET support
//    case eGet:
//      break;
/*    case eMove:
        // should we also send stuff from FIND ?
        // E: Move PresCtx but no Find (accepting for now)
        pc.SetPresentationContextID( ePatientRootQueryRetrieveInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::PatientRootQueryRetrieveInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        // move
        pc.SetPresentationContextID( ePatientRootQueryRetrieveInformationModelMOVE );
        as.SetNameFromUID( gdcm::UIDs::PatientRootQueryRetrieveInformationModelMOVE );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID( eStudyRootQueryRetrieveInformationModelFIND );
        as.SetNameFromUID( gdcm::UIDs::StudyRootQueryRetrieveInformationModelFIND );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
        pc.SetPresentationContextID( eStudyRootQueryRetrieveInformationModelMOVE );
        as.SetNameFromUID( gdcm::UIDs::StudyRootQueryRetrieveInformationModelMOVE );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
      break;*/
    case eStore:
      std::string uidName;
        pc.SetPresentationContextID( PresentationContext::AssignPresentationContextID(inDS, uidName) );
        as.SetNameFromUIDString( uidName );
        pc.SetAbstractSyntax( as );
        pcVector.push_back(pc);
      break;
  }
  mConnection->SetPresentationContexts(pcVector);


  //now, try to establish a connection by starting the transition table and the event loop.
  //here's the thing
  //if there's nothing on the event loop, assume that it's done & the function can exit.
  //otherwise, keep rolling the event loop
  ULEvent theEvent(eAASSOCIATERequestLocalUser, NULL);
  std::vector<gdcm::DataSet> empty;
  EStateID theState = RunEventLoop(theEvent, empty, mConnection, false);

  return (theState == eSta6TransferReady);//ie, finished the transitions
}



/// returns true for above reasons, but contains the special 'move' port
bool ULConnectionManager::EstablishConnectionMove(const std::string& inAETitle, const std::string& inConnectAETitle,
  const std::string& inComputerName, const long& inIPAddress,
  const unsigned short& inConnectPort, const double& inTimeout,
  const unsigned short& inReturnPort, const gdcm::DataSet& inDS){


  //generate a ULConnectionInfo object
  UserInformation userInfo;
  ULConnectionInfo connectInfo;
  if (inConnectAETitle.size() > 16) return false;//too long an AETitle, probably need better failure message
  if (inAETitle.size() > 16) return false; //as above
  if (!connectInfo.Initialize(userInfo,inAETitle.c_str(), inConnectAETitle.c_str(),
    inIPAddress, inReturnPort, inComputerName)){
    return false;
  }

  if (mSecondaryConnection != NULL){
    delete mSecondaryConnection;
  }
  mSecondaryConnection = new ULConnection(connectInfo);

  mSecondaryConnection->GetTimer().SetTimeout(inTimeout);


  //generate a ULConnectionInfo object
  UserInformation userInfo2;
  ULConnectionInfo connectInfo2;
  if (inConnectAETitle.size() > 16) return false;//too long an AETitle, probably need better failure message
  if (inAETitle.size() > 16) return false; //as above
  if (!connectInfo2.Initialize(userInfo2, inConnectAETitle.c_str(),
    inAETitle.c_str(), inIPAddress, inConnectPort, inComputerName)){
    return false;
  }

  if (mConnection!= NULL){
    delete mConnection;
  }
  mConnection = new ULConnection(connectInfo2);

  mConnection->GetTimer().SetTimeout(inTimeout);


  // Warning PresentationContextID is important
  // this is a sort of uniq key used by the recevier. Eg.
  // if one push_pack
  //  (1, Secondary)
  //  (1, Verification)
  // Then the last one is prefered (DCMTK 3.5.5)

  // The following only works for C-STORE / C-ECHO
  // however it does not make much sense to add a lot of abstract syntax
  // when doing only C-ECHO.
  // FIXME is there a way to know here if we are in C-ECHO ?
  //there is now!
  //the presentation context will now be part of the connection, so that this
  //initialization for the association-rq will use parameters from the connection

  gdcm::network::AbstractSyntax as;

  std::vector<PresentationContext> pcVector;
  PresentationContext pc;
  gdcm::network::TransferSyntax_ ts;
  ts.SetNameFromUID( gdcm::UIDs::ImplicitVRLittleEndianDefaultTransferSyntaxforDICOM );
  pc.AddTransferSyntax( ts );
  ts.SetNameFromUID( gdcm::UIDs::ExplicitVRLittleEndian );
  //pc.AddTransferSyntax( ts ); // we do not support explicit (mm)
  // should we also send stuff from FIND ?
  // E: Move PresCtx but no Find (accepting for now)
  pc.SetPresentationContextID( ePatientRootQueryRetrieveInformationModelFIND );
  as.SetNameFromUID( gdcm::UIDs::PatientRootQueryRetrieveInformationModelFIND );
  pc.SetAbstractSyntax( as );
  pcVector.push_back(pc);
  // move
  pc.SetPresentationContextID( ePatientRootQueryRetrieveInformationModelMOVE );
  as.SetNameFromUID( gdcm::UIDs::PatientRootQueryRetrieveInformationModelMOVE );
  pc.SetAbstractSyntax( as );
  pcVector.push_back(pc);
  pc.SetPresentationContextID( eStudyRootQueryRetrieveInformationModelFIND );
  as.SetNameFromUID( gdcm::UIDs::StudyRootQueryRetrieveInformationModelFIND );
  pc.SetAbstractSyntax( as );
  pcVector.push_back(pc);
  pc.SetPresentationContextID( eStudyRootQueryRetrieveInformationModelMOVE );
  as.SetNameFromUID( gdcm::UIDs::StudyRootQueryRetrieveInformationModelMOVE );
  pc.SetAbstractSyntax( as );
  pcVector.push_back(pc);
  mConnection->SetPresentationContexts(pcVector);


  //now, try to establish a connection by starting the transition table and the event loop.
  //here's the thing
  //if there's nothing on the event loop, assume that it's done & the function can exit.
  //otherwise, keep rolling the event loop
  ULEvent theEvent(eAASSOCIATERequestLocalUser, NULL);
  std::vector<gdcm::DataSet> empty;
  EStateID theState = RunEventLoop(theEvent, empty, mConnection, false);
  return (theState == eSta6TransferReady);//ie, finished the transitions
}

//send the Data PDU associated with Echo (ie, a default DataPDU)
//this lets the user confirm that the connection is alive.
//the user should look to cout to see the response of the echo command
std::vector<PresentationDataValue> ULConnectionManager::SendEcho(){

  std::vector<BasePDU*> theDataPDU = PDUFactory::CreateCEchoPDU();//pass NULL for C-Echo
  ULEvent theEvent(ePDATArequest, theDataPDU);

  std::vector<gdcm::DataSet> empty;
  EStateID theState = RunEventLoop(theEvent, empty, mConnection, false);
  //theEvent should contain the PDU for the echo!

  if (theState == eSta6TransferReady){//ie, finished the transitions
    return PDUFactory::GetPDVs(theEvent.GetPDUs());
  } else {
    std::vector<PresentationDataValue> empty;
    return empty;
  }
}

std::vector<gdcm::DataSet>  ULConnectionManager::SendMove(BaseRootQuery* inRootQuery)
{
  std::vector<BasePDU*> theDataPDU = PDUFactory::CreateCMovePDU( *mConnection, inRootQuery );
  ULEvent theEvent(ePDATArequest, theDataPDU);

  std::vector<gdcm::DataSet> theResult;
  EStateID theState = RunMoveEventLoop(theEvent, theResult);
  return theResult;
}
std::vector<gdcm::DataSet> ULConnectionManager::SendFind(BaseRootQuery* inRootQuery)
{
  std::vector<BasePDU*> theDataPDU = PDUFactory::CreateCFindPDU( *mConnection, inRootQuery );
  ULEvent theEvent(ePDATArequest, theDataPDU);

  std::vector<gdcm::DataSet> theResult;
  EStateID theState = RunEventLoop(theEvent, theResult, mConnection, false);
  return theResult;
}

std::vector<gdcm::DataSet> ULConnectionManager::SendStore(gdcm::DataSet *inDataSet)
{
  std::vector<BasePDU*> theDataPDU = PDUFactory::CreateCStoreRQPDU(inDataSet );
  ULEvent theEvent(ePDATArequest, theDataPDU);

  std::vector<gdcm::DataSet> theResult;
  EStateID theState = RunEventLoop(theEvent, theResult, mConnection, false);
  return theResult;
}

bool ULConnectionManager::BreakConnection(const double& inTimeOut){
  BasePDU* thePDU = PDUFactory::ConstructReleasePDU();
  ULEvent theEvent(eARELEASERequest, thePDU);
  mConnection->GetTimer().SetTimeout(inTimeOut);

  std::vector<gdcm::DataSet> empty;
  EStateID theState = RunEventLoop(theEvent, empty, mConnection, false);
  return (theState == eSta1Idle);//ie, finished the transitions
}

void ULConnectionManager::BreakConnectionNow(){
  BasePDU* thePDU = PDUFactory::ConstructAbortPDU();
  ULEvent theEvent(eAABORTRequest, thePDU);

  std::vector<gdcm::DataSet> empty;
  EStateID theState = RunEventLoop(theEvent, empty, mConnection, false);
}

//event handler loop for move-- will interweave the two event loops,
//one for storescp and the other for movescu.  Perhaps complicated, but
//avoids starting a second process.
EStateID ULConnectionManager::RunMoveEventLoop(ULEvent& currentEvent, std::vector<gdcm::DataSet>& outDataSet){
  EStateID theState = eStaDoesNotExist;
  bool waitingForEvent;
  EEventID raisedEvent;

  bool receivingData = false;
  bool justWaiting = false;
  //when receiving data from a find, etc, then justWaiting is true and only receiving is done
  //eventually, could add cancel into the mix... but that would be through a callback or something similar
  do {
    if (!justWaiting){
      mTransitions.HandleEvent(currentEvent, *mConnection, waitingForEvent, raisedEvent);
    }

    theState = mConnection->GetState();
    std::istream &is = *mConnection->GetProtocol();
    std::ostream &os = *mConnection->GetProtocol();

  // When doing a C-MOVE we receive the Requested DataSet over
  // another channel (technically this is send to an SCP)
  // in our case we use another port to receive it.
//#if 0
//    <<<<<<< HEAD

    /*
    if (mSecondaryConnection->GetProtocol() == NULL){
      //establish the connection
      mSecondaryConnection->InitializeIncomingConnection();
    }
    if (mSecondaryConnection->GetState()== eSta1Idle ||
      mSecondaryConnection->GetState() == eSta2Open){
      EStateID theCStoreStateID;
      ULEvent theCStoreEvent(eEventDoesNotExist, NULL);//have to fill this in, we're in passive mode now
      theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
    }


#if 1
    EStateID theCStoreStateID;
    ULEvent theCStoreEvent(eEventDoesNotExist, NULL);//have to fill this in, we're in passive mode now
    //now, get data from across the network
    theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
//=======
#if 1
                if (mSecondaryConnection->GetProtocol() == NULL){
                  //establish the connection
                  mSecondaryConnection->InitializeIncomingConnection();
                }
                if (mSecondaryConnection->GetState()== eSta1Idle ||
                  mSecondaryConnection->GetState() == eSta2Open){
                  EStateID theCStoreStateID;
                  ULEvent theCStoreEvent(eEventDoesNotExist, NULL);//have to fill this in, we're in passive mode now
                  theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
                }
#endif


#if 0
  gdcm::network::AReleaseRPPDU rel;
  rel.Write( *mSecondaryConnection->GetProtocol() );
  mSecondaryConnection->GetProtocol()->flush();
//>>>>>>> 0bad65172d78ba1c13fb98c8163e6177befc4dda
#endif
#if 1
    ULEvent theCStoreEvent2(eARELEASEResponse, NULL);//have to fill this in, we're in passive mode now
    //now, get data from across the network
    theCStoreStateID = RunEventLoop(theCStoreEvent2, outDataSet, mSecondaryConnection, true);
//    gdcm::network::AReleaseRPPDU rel;
//    rel.Write( *mSecondaryConnection->GetProtocol() );
//    mSecondaryConnection->GetProtocol()->flush();
#endif

*/
    //just as for the regular event loop, but we have to alternate between the connections.
    //it may be that nothing comes back over the is connection, but lots over the
    //isSCP connection.  So, if is fails, meh.  But if isSCP fails, that's not so meh.
    //we care only about the datasets coming back from isSCP, ultimately, though the datasets
    //from is will contain progress info.
    std::vector<BasePDU*> incomingPDUs;
    if (waitingForEvent){
      while (waitingForEvent)
        {//loop for reading in the events that come down the wire
        uint8_t itemtype = 0x0;
        is.read( (char*)&itemtype, 1 );

        BasePDU* thePDU = PDUFactory::ConstructPDU(itemtype);
        if (thePDU != NULL)
          {
          incomingPDUs.push_back(thePDU);
          thePDU->Read(is);
          std::cout << "PDU code: " << static_cast<int>(itemtype) << std::endl;
          thePDU->Print(std::cout);
          if (thePDU->IsLastFragment()) waitingForEvent = false;
          }
        else
          {
          waitingForEvent = false; //because no PDU means not waiting anymore
          }
        }
      //now, we have to figure out the event that just happened based on the PDU that was received.
      if (!incomingPDUs.empty())
        {
        currentEvent.SetEvent(PDUFactory::DetermineEventByPDU(incomingPDUs[0]));
        currentEvent.SetPDU(incomingPDUs);
        if (mConnection->GetTimer().GetHasExpired())
          {
          currentEvent.SetEvent(eARTIMTimerExpired);
          }
        if (theState == eSta6TransferReady){//ie, finished the transitions
          //with find, the results now come down the wire.
          //the pdu we already have from the event will tell us how many to expect.
          uint32_t pendingDE1, pendingDE2, success, theVal;
          pendingDE1 = 0xff01;
          pendingDE2 = 0xff00;
          success = 0x0000;
          theVal = pendingDE1;
          DataSet theRSP = PresentationDataValue::ConcatenatePDVBlobs(PDUFactory::GetPDVs(currentEvent.GetPDUs()));
          if (theRSP.FindDataElement(gdcm::Tag(0x0, 0x0900))){
            gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0900));
            gdcm::Attribute<0x0,0x0900> at;
            at.SetFromDataElement( de );
            theVal = at.GetValues()[0];
            //if theVal is Pending or Success, then we need to enter the loop below,
            //because we need the data PDUs.
            //so, the loop below is a do/while loop; there should be at least a second packet
            //with the dataset, even if the status is 'success'
            //success == 0000H
          }
          if (theVal != pendingDE1 && theVal != pendingDE2 && theVal != success){
            //check for other error fields
            ByteValue *err1 = NULL, *err2 = NULL;
            std::cout << "Transfer failed with code " << theVal << std::endl;
            switch (theVal){
              case 0xA701:
                std::cout << "Refused: Out of Resources Unable to calculate number of matches" << std::endl;
                break;
              case 0xA702:
                std::cout << "Refused: Out of Resources Unable to perform sub-operations" << std::endl;
                break;
              case 0xA801:
                std::cout << "Refused: Move Destination unknown" << std::endl;
                break;
              case 0xA900:
                std::cout << "Identifier does not match SOP Class" << std::endl;
                break;
              case 0xAA00:
                std::cout << "None of the frames requested were found in the SOP Instance" << std::endl;
                break;
              case 0xAA01:
                std::cout << "Unable to create new object for this SOP class" << std::endl;
                break;
              case 0xAA02:
                std::cout << "Unable to extract frames" << std::endl;
                break;
              case 0xAA03:
                std::cout << "Time-based request received for a non-time-based original SOP Instance. " << std::endl;
                break;
              case 0xAA04:
                std::cout << "Invalid Request" << std::endl;
                break;
              case 0xFE00:
                std::cout << "Sub-operations terminated due to Cancel Indication" << std::endl;
                break;
              case 0xB000:
                std::cout << "Sub-operations Complete One or more Failures or Warnings" << std::endl;
                break;
              default:
                std::cout << "Unable to process" << std::endl;
                break;
            }
            if (theRSP.FindDataElement(gdcm::Tag(0x0,0x0901))){
              gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0901));
              err1 = de.GetByteValue();
              std::cout << " Tag 0x0,0x901 reported as " << *err1 << std::endl;
            }
            if (theRSP.FindDataElement(gdcm::Tag(0x0,0x0902))){
              gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0902));
              err2 = de.GetByteValue();
              std::cout << " Tag 0x0,0x902 reported as " << *err2 << std::endl;
            }
          }
          receivingData = false;
          justWaiting = false;
          if (theVal == pendingDE1 || theVal == pendingDE2) {
            receivingData = true; //wait for more data as more PDUs (findrsps, for instance)
            justWaiting = true;
            waitingForEvent = true;

            //ok, if we're pending, then let's open the cstorescp connection here
            //(if it's not already open), and then from here start a storescp event loop.
            //just don't listen to the cmove event loop until this is done.
            //could cause a pileup on the main connection, I suppose.
            //could also report the progress here, if we liked.
            if (theRSP.FindDataElement(gdcm::Tag(0x0,0x0100))){
              gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0100));
              gdcm::Attribute<0x0,0x0100> at;
              at.SetFromDataElement( de );
              uint32_t theCommandCode = at.GetValues()[0];
              if (theCommandCode == 0x8021){//cmove response, so prep the retrieval loop on the back connection
                if (mSecondaryConnection->GetProtocol() == NULL){
                  //establish the connection
                  mSecondaryConnection->InitializeIncomingConnection();
                }
                EStateID theCStoreStateID = eSta6TransferReady;
                if (mSecondaryConnection->GetState()== eSta1Idle ||
                  mSecondaryConnection->GetState() == eSta2Open){
                  ULEvent theCStoreEvent(eEventDoesNotExist, NULL);//have to fill this in, we're in passive mode now
                  theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
                }
                bool dataSetCountIncremented = true;//false once the number of incoming datasets doesn't change.
                while (theCStoreStateID == eSta6TransferReady && dataSetCountIncremented){
                  ULEvent theCStoreEvent(eEventDoesNotExist, NULL);//have to fill this in, we're in passive mode now
                  //now, get data from across the network
                  int theNumDataSets = outDataSet.size();
                  theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
                  dataSetCountIncremented = false;
                  if (outDataSet.size() > theNumDataSets)
                    dataSetCountIncremented = true;
                }
                //force the abort from our side
              //  ULEvent theCStoreEvent(eAABORTRequest, NULL);//have to fill this in, we're in passive mode now
              //  theCStoreStateID = RunEventLoop(theCStoreEvent, outDataSet, mSecondaryConnection, true);
              }
            } else {//not dealing with cmove progress updates, apparently
              //keep looping if we haven't succeeded or failed; these are the values for 'pending'
              //first, dynamically cast that pdu in the event
              //should be a data pdu
              //then, look for tag 0x0,0x900

              //only add datasets that are _not_ part of the network response
              std::vector<gdcm::DataSet> final;
              std::vector<BasePDU*> theData;
              BasePDU* thePDU;//outside the loop for the do/while stopping condition
              bool interrupted = false;
              do {
                uint8_t itemtype = 0x0;
                is.read( (char*)&itemtype, 1 );
                //what happens if nothing's read?
                thePDU = PDUFactory::ConstructPDU(itemtype);
                if (itemtype != 0x4 && thePDU != NULL){ //ie, not a pdatapdu
                  std::vector<BasePDU*> interruptingPDUs;
                  currentEvent.SetEvent(PDUFactory::DetermineEventByPDU(interruptingPDUs[0]));
                  currentEvent.SetPDU(interruptingPDUs);
                  interrupted= true;
                  break;
                }
                if (thePDU != NULL){
                  thePDU->Read(is);
                  theData.push_back(thePDU);
                } else{
                  break;
                }
                //!!!need to handle incoming PDUs that are not data, ie, an abort
              } while(/*!is.eof() &&*/ !thePDU->IsLastFragment());
              if (!interrupted){//ie, if the remote server didn't hang up
                DataSet theCompleteFindResponse =
                  PresentationDataValue::ConcatenatePDVBlobs(PDUFactory::GetPDVs(theData));
                //note that it's the responsibility of the event to delete the PDU in theFindRSP
                for (int i = 0; i < theData.size(); i++){
                  delete theData[i];
                }
                outDataSet.push_back(theCompleteFindResponse);
              }
            }
          }
        }
      } else {
        raisedEvent = eEventDoesNotExist;
        waitingForEvent = false;
      }
    }
    else {
      currentEvent.SetEvent(raisedEvent);//actions that cause transitions in the state table
      //locally just raise local events that will therefore cause the trigger to be pulled.
    }
  } while (currentEvent.GetEvent() != eEventDoesNotExist &&
    theState != eStaDoesNotExist && theState != eSta13AwaitingClose && theState != eSta1Idle &&
    (theState != eSta6TransferReady || (theState == eSta6TransferReady && receivingData )));
  //stop when the AE is done, or when ready to transfer data (ie, the next PDU should be sent in),
  //or when the connection is idle after a disconnection.
  //or, if in state 6 and receiving data, until all data is received.

  return theState;
}


//event handler loop.
//will just keep running until the current event is nonexistent.
//at which point, it will return the current state of the connection
//to do this, execute an event, and then see if there's a response on the
//incoming connection (with a reasonable amount of timeout).
//if no response, assume that the connection is broken.
//if there's a response, then yay.
//note that this is the ARTIM timeout event
EStateID ULConnectionManager::RunEventLoop(ULEvent& currentEvent, std::vector<gdcm::DataSet>& outDataSet,
        ULConnection* inWhichConnection, const bool& startWaiting = false){
  EStateID theState = eStaDoesNotExist;
  bool waitingForEvent = startWaiting;//overwritten if not starting waiting, but if waiting, then wait
  EEventID raisedEvent;

  bool receivingData = false;
  //bool justWaiting = startWaiting;
  //not sure justwaiting is useful; for now, go back to waiting for event

  //when receiving data from a find, etc, then justWaiting is true and only receiving is done
  //eventually, could add cancel into the mix... but that would be through a callback or something similar
  do {
    if (!waitingForEvent){//justWaiting){
      mTransitions.HandleEvent(currentEvent, *inWhichConnection, waitingForEvent, raisedEvent);
      //this gathering of the state is for scus that have just sent out a request
      theState = inWhichConnection->GetState();
    }
    std::istream &is = *inWhichConnection->GetProtocol();
    std::ostream &os = *inWhichConnection->GetProtocol();

    BasePDU* theFirstPDU = NULL;// the first pdu read in during this event loop,
    //used to make sure the presentation context ID is correct

    //read the connection, as that's an event as well.
    //waiting for an object to come back across the connection, so that it can get handled.
    //ie, accept, reject, timeout, etc.
    //of course, if the connection is down, just leave the loop.
    //also leave the loop if nothing's waiting.
    //use the PDUFactory to create the appropriate pdu, which has its own
    //internal mechanisms for handling itself (but will, of course, be put inside the event object).
    //but, and here's the important thing, only read on the socket when we should.
    std::vector<BasePDU*> incomingPDUs;
    if (waitingForEvent){
      while (waitingForEvent){//loop for reading in the events that come down the wire
        uint8_t itemtype = 0x0;
        try {
          is.read( (char*)&itemtype, 1 );
          //what happens if nothing's read?
          theFirstPDU = PDUFactory::ConstructPDU(itemtype);
          if (theFirstPDU != NULL){
            incomingPDUs.push_back(theFirstPDU);
            theFirstPDU->Read(is);
            std::cout << "PDU code: " << static_cast<int>(itemtype) << std::endl;
            theFirstPDU->Print(std::cout);
            if (theFirstPDU->IsLastFragment()) waitingForEvent = false;
          } else {
            waitingForEvent = false; //because no PDU means not waiting anymore
          }
        }
        catch (...){
          //handle the exception, which is basically that nothing came in over the pipe.
        }
      }
      //now, we have to figure out the event that just happened based on the PDU that was received.
      //this state gathering is for scps, especially the cstore for cmove.
      theState = inWhichConnection->GetState();
      if (!incomingPDUs.empty()){
        currentEvent.SetEvent(PDUFactory::DetermineEventByPDU(incomingPDUs[0]));
        currentEvent.SetPDU(incomingPDUs);
        //here's the scp handling code
        if (mConnection->GetTimer().GetHasExpired()){
          currentEvent.SetEvent(eARTIMTimerExpired);
        }
        switch(currentEvent.GetEvent()){
          case ePDATATFPDU:
            {
            //if (theState == eSta6TransferReady){//ie, finished the transitions
              //with find, the results now come down the wire.
              //the pdu we already have from the event will tell us how many to expect.
              uint32_t pendingDE1, pendingDE2, success, theVal;
              pendingDE1 = 0xff01;
              pendingDE2 = 0xff00;
              success = 0x0000;
              theVal = pendingDE1;
              uint32_t theCommandCode = 0;//for now, a nothing value
              DataSet theRSP = PresentationDataValue::ConcatenatePDVBlobs(PDUFactory::GetPDVs(currentEvent.GetPDUs()));
              if (theRSP.FindDataElement(gdcm::Tag(0x0, 0x0900))){
                gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0900));
                gdcm::Attribute<0x0,0x0900> at;
                at.SetFromDataElement( de );
                theVal = at.GetValues()[0];
                //if theVal is Pending or Success, then we need to enter the loop below,
                //because we need the data PDUs.
                //so, the loop below is a do/while loop; there should be at least a second packet
                //with the dataset, even if the status is 'success'
                //success == 0000H
              }
              //check to see if this is a cstorerq
              if (theRSP.FindDataElement(gdcm::Tag(0x0, 0x0100))){
                gdcm::DataElement de2 = theRSP.GetDataElement(gdcm::Tag(0x0,0x0100));
                gdcm::Attribute<0x0,0x0100> at2;
                at2.SetFromDataElement( de2 );
                theCommandCode = at2.GetValues()[0];
              }

              if (theVal != pendingDE1 && theVal != pendingDE2 && theVal != success){
                //check for other error fields
                ByteValue *err1 = NULL, *err2 = NULL;
                std::cout << "Transfer failed with code " << theVal << std::endl;
                if (theRSP.FindDataElement(gdcm::Tag(0x0,0x0901))){
                  gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0901));
                  err1 = de.GetByteValue();
                  std::cout << " Tag 0x0,0x901 reported as " << *err1 << std::endl;
                }
                if (theRSP.FindDataElement(gdcm::Tag(0x0,0x0902))){
                  gdcm::DataElement de = theRSP.GetDataElement(gdcm::Tag(0x0,0x0902));
                  err2 = de.GetByteValue();
                  std::cout << " Tag 0x0,0x902 reported as " << *err2 << std::endl;
                }
              }




              receivingData = false;
              //justWaiting = false;
              if (theVal == pendingDE1 || theVal == pendingDE2) {
                receivingData = true; //wait for more data as more PDUs (findrsps, for instance)
                //justWaiting = true;
                waitingForEvent = true;
              }
              if (theVal == pendingDE1 || theVal == pendingDE2 /*|| theVal == success*/){//keep looping if we haven't succeeded or failed; these are the values for 'pending'
                //first, dynamically cast that pdu in the event
                //should be a data pdu
                //then, look for tag 0x0,0x900

                //only add datasets that are _not_ part of the network response
                std::vector<gdcm::DataSet> final;
                std::vector<BasePDU*> theData;
                BasePDU* thePDU;//outside the loop for the do/while stopping condition
                bool interrupted = false;
                do {
                  uint8_t itemtype = 0x0;
                  is.read( (char*)&itemtype, 1 );
                  //what happens if nothing's read?
                  thePDU = PDUFactory::ConstructPDU(itemtype);
                  if (itemtype != 0x4 && thePDU != NULL){ //ie, not a pdatapdu
                    std::vector<BasePDU*> interruptingPDUs;
                    interruptingPDUs.push_back(thePDU);
                    currentEvent.SetEvent(PDUFactory::DetermineEventByPDU(interruptingPDUs[0]));
                    currentEvent.SetPDU(interruptingPDUs);
                    interrupted= true;
                    break;
                  }
                  if (thePDU != NULL){
                    thePDU->Read(is);
                    theData.push_back(thePDU);
                  } else{
                    break;
                  }
                  //!!!need to handle incoming PDUs that are not data, ie, an abort
                } while(!thePDU->IsLastFragment());
                if (!interrupted){//ie, if the remote server didn't hang up
                  DataSet theCompleteFindResponse =
                    PresentationDataValue::ConcatenatePDVBlobs(PDUFactory::GetPDVs(theData));
                  //note that it's the responsibility of the event to delete the PDU in theFindRSP
                  for (int i = 0; i < theData.size(); i++){
                    delete theData[i];
                  }
                  outDataSet.push_back(theCompleteFindResponse);

                  if (theCommandCode == 1){//if we're doing cstore scp stuff, send information back along the connection.
                    std::vector<BasePDU*> theCStoreRSPPDU = PDUFactory::CreateCStoreRSPPDU(&theRSP, theFirstPDU);//pass NULL for C-Echo
                    //send them directly back over the connection
                    //ideall, should go through the transition table, but we know this should work
                    //and it won't change the state (unless something breaks?, but then an exception should throw)
                    std::vector<BasePDU*>::iterator itor;
                    for (itor = theCStoreRSPPDU.begin(); itor < theCStoreRSPPDU.end(); itor++){
                      (*itor)->Write(*inWhichConnection->GetProtocol());
                    }

                    inWhichConnection->GetProtocol()->flush();

                    // FIXME added MM / Oct 30 2010
                    gdcm::network::AReleaseRPPDU rel;
                    //rel.Write( *inWhichConnection->GetProtocol() );
                    //inWhichConnection->GetProtocol()->flush();

                    receivingData = false; //gotta get data on the other connection for a cmove
                  }
                }
              }
            }
            break;
            case eARELEASERequest://process this via the transition table
              waitingForEvent = false;
              break;
            case eAABORTRequest:
              waitingForEvent = false;
              inWhichConnection->StopProtocol();
              break;
            case eASSOCIATE_ACPDUreceived:
              waitingForEvent = false;
              break;
          }
        }
      //} else {
      //  raisedEvent = eEventDoesNotExist;
      //  waitingForEvent = false;
      //}
    }
    else {
      currentEvent.SetEvent(raisedEvent);//actions that cause transitions in the state table
      //locally just raise local events that will therefore cause the trigger to be pulled.
    }
  } while (currentEvent.GetEvent() != eEventDoesNotExist &&
    theState != eStaDoesNotExist && theState != eSta13AwaitingClose && theState != eSta1Idle &&
    (theState != eSta6TransferReady || (theState == eSta6TransferReady && receivingData )));
  //stop when the AE is done, or when ready to transfer data (ie, the next PDU should be sent in),
  //or when the connection is idle after a disconnection.
  //or, if in state 6 and receiving data, until all data is received.

  return theState;
}
