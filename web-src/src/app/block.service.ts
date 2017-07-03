import { Injectable } from '@angular/core';

@Injectable()
export class BlockService {
  websocket;
  config: JSONS.ConfigObject;
  newBlock: JSONS.NewBlockObject;
  lastWinner: JSONS.LastWinnerObject;
  progress = 0;
  nonces: Array<JSONS.NonceObject> = [];
  plots: Array<JSONS.PlotDirObject> = [];
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


        switch (response['type']) {
          case 'new block':
            this.newBlock = response;
            this.nonces = [];
            this.plots = [];
            break;
          case 'nonce found':
            this.addOrUpdateNonce(response);
            break;
          case 'nonce confirmed':
            this.addOrUpdateNonce(response);
            this.confirmedSound.play();
            break;
          case 'nonce submitted':
            this.addOrUpdateNonce(response);
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
            console.log(response.type, response);
            break;
          case 'plotdir-progress':
            this.addOrUpdatePlot(response);
            break;
          case 'plotdirs-rescan':
            console.log(response.type, response);
            break;
          default:
            //        showMessage(response);
            if (response.type != '7')
              console.log(response.type, response);
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


  private addOrUpdatePlot(plot: JSONS.PlotDirObject) {
    const p = this.plots.filter(x => x.dir === plot.dir);
    if (p.length > 0) {
      p[0].value = plot.value;
      plot = p[0];
    } else {
      this.plots.push(plot);
    }
    console.log(plot.value);
    if (plot.value.toString() === '100') {
      setTimeout(() => {
        plot.closed = true;
      }, 2000 + (Math.random() * 1000));
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
