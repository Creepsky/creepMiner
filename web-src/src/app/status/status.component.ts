import { Component, OnInit } from '@angular/core';
import { BlockService } from '../block.service';

@Component({
  selector: 'app-status',
  templateUrl: './status.component.html',
  styleUrls: ['./status.component.css']
})
export class StatusComponent implements OnInit {
  public lineChartData = [
    { data: [], label: 'Deadlines' }
  ];
  public lineChartLabels: Array<any>;
  public lineChartOptions: any = {
    responsive: true,
    scales: {
      yAxes: [{
        ticks: {
          callback: value => {
            let first = Math.floor(value / 60 / 60);
            let second = Math.round(value / 60) - first * 60;
            let sufix1 = 'h';
            let sufix2 = 'm';
            if (first > 24.0) {
              first = Math.floor(first / 24);
              second = Math.round(Math.round(value / 3600)) - first * 24;
              sufix1 = 'd';
              sufix2 = 'h';
            }
            return (first + ' ' + sufix1 + ' ' + second + ' ' + sufix2);
          }
        }
      }]
    },
    tooltips: {
      enabled: true,
      mode: 'single',
      callbacks: {
        label: tooltipItems => {
          const value = tooltipItems.yLabel;
          let first = Math.floor(value / 60 / 60);
          let second = Math.round(value / 60) - first * 60;
          let sufix1 = 'h';
          let sufix2 = 'm';
          if (first > 24.0) {
            first = Math.floor(first / 24);
            second = Math.round(Math.round(value / 3600)) - first * 24;
            sufix1 = 'd';
            sufix2 = 'h';
          }
          return (first + ' ' + sufix1 + ' ' + second + ' ' + sufix2);
        }
      }
    }
  };
  public lineChartColors: Array<any> = [
    { // dark grey
      backgroundColor: 'rgba(77,83,96,0.2)',
      borderColor: 'rgba(77,83,96,1)',
      pointBackgroundColor: 'rgba(77,83,96,1)',
      pointBorderColor: '#fff',
      pointHoverBackgroundColor: '#fff',
      pointHoverBorderColor: 'rgba(77,83,96,1)'
    }
  ];
  public sounds = {
    block: false,
    confirmation: false
  }

  toggleSound(type: 'block' | 'confirmation') {
    this.sounds[type] = !this.sounds[type];
    localStorage.setItem(type + 'Sound', this.sounds[type].toString());
  }

  constructor(
    public b: BlockService
  ) {
  }


  ngOnInit() {
    for (const p in this.sounds) {
      if (this.sounds.hasOwnProperty(p)) {
        this.sounds[p] = (localStorage.getItem(p + 'Sound') === 'true');
      }
    }

    this.b.connect();
    this.b.newBlock$.subscribe(nb => {
      const bestDl = <Array<Array<number>>>nb['bestDeadlines'];
      this.lineChartLabels = bestDl.map(x => x[0]);
      this.lineChartData[0].data = bestDl.map(x => x[1]);
    });
    this.b.newConfirmation$.subscribe(nc => {
      const data = this.lineChartData[0].data;
      if (this.lineChartLabels[this.lineChartLabels.length - 1] === this.b.newBlock.block) {
        //  data[data.length - 1] = this.bestDeadline().deadlineNum;
      } else {
        this.lineChartLabels.push(this.b.newBlock.block);
        //       data.push(this.bestDeadline().deadlineNum);
        setTimeout(() => { }, 0); // ?
      }
    });
  }

  round(n: number): number {
    return Math.round(n);
  }

  countNonces(plotDir?: JSONS.PlotDirObject): number {
    let sum = 0;
    (plotDir ? [plotDir] : this.b.plots).forEach(p => {
      p.plotfiles.forEach(pf => {
        sum += pf.nonces.length;
      })
    });
    return sum;
  }


  bestDeadline(): JSONS.NonceObject {
    const nonces: Array<JSONS.NonceObject> = [];
    this.b.plots.forEach(p => {
      p.plotfiles.forEach(pf => {
        nonces.push(...pf.nonces);
      })
    });

    console.log(nonces);

    const bestDl = Math.min(...nonces.map(x => x.deadlineNum));
    console.log('best', bestDl);
    if (bestDl !== Infinity) {
      return nonces.filter(n => n.deadlineNum === bestDl)[0];
    } else {
      return null;
    }
  }

}
