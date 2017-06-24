import { Injectable } from '@angular/core';

@Injectable()
export class BlockService {
  websocket;
  config;

  constructor() { }



  connectBlock() {
    this.connect((msg) => {

      const data = msg['data'];

      if (data) {
        if (data === 'ping') {
          return;
        }

        const response = JSON.parse(data);
        console.log('ws', response);
        switch (response['type']) {
          case 'new block':
            //    newBlock(response);
            break;
          case 'nonce found':
            //     nonceFound(response);
            break;
          case 'nonce confirmed':
            //     addOrConfirm(response);
            ////     checkAddBestRound(BigInteger(response['deadlineNum']), response['deadline']);
            //    checkAddBestOverall(BigInteger(response['deadlineNum']), response['deadline']);
            break;
          case 'nonce submitted':
            //    addOrSubmit(response);
            break;
          case 'config':
            this.config = response;
            break;
          case 'progress':
            //    setOverallProgress(response['value']);
            break;
          case 'lastWinner':
            //    setLastWinner(response);
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



  connect(onMessage) {
    if ('WebSocket' in window) {
      if (this.websocket) {
        this.websocket.close();
      }

      this.websocket = new WebSocket('ws://localhost:8080/');
      this.websocket.onmessage = onMessage;
    } else {
      this.websocket = null;
    }
  }

}
