import { Injectable } from '@angular/core';
import { Subject } from 'rxjs/Subject';

@Injectable()
export class BlockService {
  websocket: WebSocket;
  config: JSONS.ConfigObject;
  newBlock: JSONS.NewBlockObject;
  lastWinner: JSONS.LastWinnerObject;
  progress = 0;

  plots: Array<JSONS.PlotDirObject> = [];
  unknownNonces: Array<JSONS.NonceObject> = [];
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

      //  console.log('sve', response.type, response);

      switch (response['type']) {
        case 'new block':
          this.newBlock = response;
          this.unknownNonces = [];
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
          this.plots = response.plotdirs;
          this.plots.forEach(p => {
            p.plotfiles.forEach(pf => {
              pf.nonces = [];
            });
            p.closed = false;
            p.progress = 0;
          });
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


  private addOrUpdatePlot(plotUpdate) {
    const p = this.plots.filter(x => x.path === plotUpdate.dir)[0];
    if (p) {
      p.progress = plotUpdate.value;
    } else {
      console.error('OVO NE MOZE');
      //  plot.nonces = [];
      // this.plots.push(plot);
    }
    if (plotUpdate.value.toString() === '100' && !p.plotfiles.some(pf => !!pf.nonces.length)) {
      setTimeout(() => {
        p.closed = true;
      }, 2000 + (Math.random() * 1000));
    }
  }

  private addOrUpdateNonce(nonce: JSONS.NonceObject) {
    const index = nonce.plotfile.lastIndexOf('/');
    const plotDir = nonce.plotfile.slice(0, index);

    let pfFound = false;

    const pushOrUpdate = (arr: Array<JSONS.NonceObject>) => {
      const ns = arr.filter(x => x.nonce === nonce.nonce)[0];
      if (ns) {
        ns.type = nonce.type;
      } else {
        nonce.deadlineNum = +nonce.deadlineNum;
        arr.push(nonce);
      }
    }

    this.plots.forEach(p => {
      const pf = p.plotfiles.filter(p2 => p2.path === nonce.plotfile)[0];
      if (pf) {
        pfFound = true;
        pushOrUpdate(pf.nonces);
      }
    });

    if (!pfFound) {
      pushOrUpdate(this.unknownNonces);
    }
  }

  connect() {
    if ('WebSocket' in window) {
      if (this.websocket) {
        return;
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
