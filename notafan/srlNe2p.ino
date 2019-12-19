// 
void  guideInfo(){
  Serial.println("Follow the below!");
  Serial.println("\tssid:androidhotspot1234(Rewrite yourap-name)");
  Serial.println("\tpswd:******** (AP password)");
  Serial.println("\tbroker:mqtt.hivemq.com(Rewrite your Broker-IP)");
  Serial.println("\tstoreid:midoriya(Rewrite your store name)");
  Serial.println("\ttableid:12(Set the table number)");
  Serial.println("\t\tnot in blank, Upper/Lower character is ignored.");
}
// get 1byte data from serial buffer
bool getData() {
  bool  retb = false;
  if(Serial.available() > 0){
    char  data = Serial.read();
    if( data == '\n' ){
      msgBuf[msgIndex] = NULL;
      msgIndex = 0;
      retb = true;
    } else {
      msgBuf[msgIndex++] = data;
    }
  }
  return  retb;
}
//
bool setInfo(){
  bool  retb = false;
  char  *pKey, *pVal;
  pKey = strtok(msgBuf, ":");
  pVal = strtok(NULL, ":");
  if( pKey == NULL ){
    return  retb;
  }
  
  if( pVal == NULL ){
    if( !strcasecmp(pKey,"PSWD") ){
      fInfoCheck |= 0x02;
      myinfo.pswd[0] = 0x00;
    }else{
      return  retb;
    }
  }else if(!strcasecmp(pKey,"SSID")){
    fInfoCheck |= 0x01;
    strcpy(myinfo.ssid, pVal);
  }else if(!strcasecmp(pKey,"PSWD")){
    fInfoCheck |= 0x02;
    strcpy(myinfo.pswd, pVal);
  }else if(!strcasecmp(pKey,"BROKER")){
    fInfoCheck |= 0x04;
    strcpy(myinfo.mqtt, pVal);
  }else if(!strcasecmp(pKey,"STOREID")){
    fInfoCheck |= 0x08;
    strcpy(myinfo.storeId, pVal);
    makeTopic();  // 1213
  }else if(!strcasecmp(pKey,"TABLEID")){
    fInfoCheck |= 0x10;
    myinfo.tableId = (byte)atoi(pVal);
    makeTopic();  // 1213
  }else{
    return  retb;
  }
  //
  retb = true;
  #ifdef  _DBG
//  if(retb){
    Serial.print("\n[Update Info]");
    Serial.print(pKey);Serial.print(" : ");Serial.println(pVal);
//  }
  #endif
  return  retb;  
}
// get info of 1 item.
bool  getInfo() {
  bool  retb = false;
  if( getData() == true ){
    if( setInfo() == false){
      Serial.println("[Warning!!]Invalid Data Form.");
      guideInfo();
    }else if(fInfoCheck == 0x1f){
      retb = true;
    }
  }
  return  retb;
}
//
void  setData2E2p(const MyInfoSet* pData){
  byte  *pByte;
  if( pData ){
    pByte = (byte*)pData;
  }else{
    pByte = (byte*)&myinfo;
  }
  // write info to e2prom
  for(int i=0;i<e2pSize;i++){
    EEPROM.write(pE2p+i, pByte[i]);
  }
  EEPROM.commit();  //12102
  #ifdef  _DBG
  Serial.println("\t\t... setData2E2p()");
  #endif
}
//
bool  getDataFromE2p(){
  bool  retb = false;
  byte  *pByte = (byte*)&myinfo;
  for(int i=0;i<e2pSize;i++){
    pByte[i] = EEPROM.read(pE2p+i);
  }
  myinfo.validmsg[7] = 0x00;  //"malkori"
  if( !strcmp(pValid, myinfo.validmsg) ){
    retb = true;
    fInfoCheck = 0x1f;
    #ifdef  _DBG
    Serial.println("Success to get Valid data.");
    sprintf(msgBuf, "\tSSID: %s", myinfo.ssid);
    Serial.println(msgBuf);
    sprintf(msgBuf, "\tPassword: %s", myinfo.pswd);
    Serial.println(msgBuf);
    sprintf(msgBuf, "\tBroker: %s", myinfo.mqtt);
    Serial.println(msgBuf);
    sprintf(msgBuf, "\tStoreID: %s", myinfo.storeId);
    Serial.println(msgBuf);
    sprintf(msgBuf, "\tTableID: %d", myinfo.tableId);
    Serial.println(msgBuf);
    #endif
  }
  return  retb;
}
