AudioSignalResClass {
 Inputs {
  IOPItemInputClass {
   id 1
   name "LLRadioEar"
   tl -256 -48
   children {
    20 21
   }
  }
  IOPItemInputClass {
   id 30
   name "LLRadioVolume"
   tl -253.584 253.558
   children {
    31
   }
  }
 }
 Ops {
  IOPItemOpConvertorClass {
   id 20
   name "LeftChannelVol"
   tl 71.065 -185.632
   children {
    22
   }
   inputs {
    ConnectionClass "1:0" {
     id 1
     port 0
    }
   }
   Default 2
   Intervals {
    IOPItemOpConvertorRange LeftChannel {
     min 1
     max 2
    }
   }
  }
  IOPItemOpConvertorClass {
   id 21
   name "RightChannelVol"
   tl 76.623 124.05
   children {
    2
   }
   inputs {
    ConnectionClass "1:0" {
     id 1
     port 0
    }
   }
   Default 2
   Intervals {
    IOPItemOpConvertorRange RightChannel {
     min 2
     max 3
    }
   }
  }
 }
 Outputs {
  IOPItemOutputClass {
   id 2
   name "RightVol"
   tl 472.213 122.741
   input 21
  }
  IOPItemOutputClass {
   id 22
   name "LeftVol"
   tl 464 -192
   input 20
  }
  IOPItemOutputClass {
   id 31
   name "ChanVol"
   tl 472.213 253.558
   input 30
  }
 }
 Input_Order {
  ItemDetailListItemClass LLRadioEar {
   Name "LLRadioEar"
   Id 1
  }
  ItemDetailListItemClass LLRadioVolume {
   Name "LLRadioVolume"
   Id 30
  }
 }
 Output_Order {
  ItemDetailListItemClass RightVol {
   Name "RightVol"
   Id 2
  }
  ItemDetailListItemClass LeftVol {
   Name "LeftVol"
   Id 22
  }
  ItemDetailListItemClass ChanVol {
   Name "ChanVol"
   Id 31
  }
 }
}