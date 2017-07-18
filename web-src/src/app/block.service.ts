import { Injectable } from '@angular/core';
import { Subject } from 'rxjs/Subject';

@Injectable()
export class BlockService {
  websocket: WebSocket;
  config: JSONS.ConfigObject;
  newBlock: JSONS.NewBlockObject;
  lastWinner: JSONS.LastWinnerObject;
  progress = 0;
  nonces: Array<JSONS.NonceObject> = [];
  plots: Array<JSONS.PlotDirObject> = [];
  confirmedSound = new Audio('assets/sms-alert-1-daniel_simon.mp3');
  private _isreconn = false;

  blockTime: Date;
  blockReadTime: Date;

  private newBlockSource = new Subject();
  newBlock$ = this.newBlockSource.asObservable();
  private newConfirmationSource = new Subject();
  newConfirmation$ = this.newConfirmationSource.asObservable();

  constructor() {
    this.confirmedSound.volume = 0.5;

    const refrFunc = () => {
      // triggers change detection every second
      setTimeout(refrFunc, 1000);
    }
    refrFunc();
  }

  getBlockReadDate(): string {
    let diff = 0;
    if (this.blockReadTime) {
      diff = this.blockReadTime.getTime() - this.blockTime.getTime();
    } else {
      diff = new Date().getTime() - this.blockTime.getTime();
    }

    const mins = Math.floor(diff / (1000 * 60));
    diff -= mins * (1000 * 60);

    const seconds = Math.floor(diff / (1000));
    diff -= seconds * (1000);

    return mins ? mins + ':' + seconds : seconds.toString();
  }

  private rcvMsg(msg) {
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
          this.blockTime = new Date();
          this.blockReadTime = null;
          this.newBlockSource.next(response);
          break;
        case 'nonce found':
          this.addOrUpdateNonce(response);
          break;
        case 'nonce confirmed':
          this.addOrUpdateNonce(response);
          this.newConfirmationSource.next(response);
          if (localStorage.getItem('confirmationSound') === 'true') {
            this.confirmedSound.play();
          }
          break;
        case 'nonce submitted':
          this.addOrUpdateNonce(response);
          break;
        case 'config':
          this.config = response;
          break;
        case 'progress':
          this.progress = response.value;
          if (response.value === 100) {
            this.blockReadTime = new Date();
          }
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
          if (response.type != '7') {
            console.log(response.type, response);
          }
          break;
      };
    }
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
    if (plot.value.toString() === '100') {
      setTimeout(() => {
        plot.closed = true;
      }, 2000 + (Math.random() * 1000));
    }
  }



  connect() {
    if ('WebSocket' in window) {
      if (this.websocket) {
        this.websocket.close();
      }

      this.websocket = new WebSocket('ws://' + window.location.hostname + ':' + window.location.port + '/');
      this.websocket.onmessage = this.rcvMsg.bind(this);

      this.websocket.onerror = this.reconnect.bind(this);
      this.websocket.onclose = this.reconnect.bind(this);
    } else {
      this.websocket = null;
    }
  }



  private reconnect() {
    if (this._isreconn) {
      return;
    }
    this._isreconn = true;
    setTimeout(() => {
      console.log('Reconnecting...');
      this._isreconn = false;
      this.connect();
    }, 3000);
  }

}
