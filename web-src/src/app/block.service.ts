import { Injectable } from '@angular/core';

@Injectable()
export class BlockService {
  websocket;
  config: JSONS.ConfigObject;
  newBlock: JSONS.NewBlockObject;
  lastWinner: JSONS.LastWinnerObject;
  progress = 0;
  nonces: Array<JSONS.NonceObject> = [];
  confirmedSound = new Audio('assets/sms-alert-1-daniel_simon.mp3');

  constructor() {
    this.confirmedSound.volume = 0.5;
  }



  connectBlock() {
    this.connect((msg) => {

      const data = msg['data'];

      if (data) {
        if (data === 'ping') {
          return;
        }
        const response = JSON.parse(data);
        console.log(response.type, response);

        switch (response['type']) {
          case 'new block':
            this.newBlock = response;
            this.nonces = [];
            break;
          case 'nonce found':
            //     nonceFound(response);
            this.addOrUpdateNonce(response);
            break;
          case 'nonce confirmed':
            this.addOrUpdateNonce(response);
            this.confirmedSound.play();
            //     addOrConfirm(response);
            ////     checkAddBestRound(BigInteger(response['deadlineNum']), response['deadline']);
            //    checkAddBestOverall(BigInteger(response['deadlineNum']), response['deadline']);
            break;
          case 'nonce submitted':
            this.addOrUpdateNonce(response);

            //    addOrSubmit(response);
            break;
          case 'config':
            this.config = response;
            break;
          case 'progress':
            this.progress = response.value;
            break;
          case 'lastWinner':
            this.lastWinner = response;
            break;
          case 'blocksWonUpdate':
            //      wonBlocks.html(reponse['blocksWon']);
            break;
          case 'plotdir-progress':
          case 'plotdirs-rescan':
            // do nothing
            break;
          default:
            //        showMessage(response);
            break;
        };
      }
    });
  }


  private addOrUpdateNonce(nonce: JSONS.NonceObject) {
    const ns = this.nonces.filter(x => x.nonce === nonce.nonce);
    if (ns.length > 0) {
      ns[0].type = nonce.type;
    } else {
      this.nonces.push(nonce);
    }
  }



  connect(onMessage) {
    if ('WebSocket' in window) {
      if (this.websocket) {
        this.websocket.close();
      }

      this.websocket = new WebSocket('ws://' + window.location.host + ':' + window.location.port + '/');
      this.websocket.onmessage = onMessage;
    } else {
      this.websocket = null;
    }
  }

}
