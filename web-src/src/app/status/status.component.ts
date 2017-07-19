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
          let value = tooltipItems.yLabel;
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
    newBlock: false,
    confirmation: false
  }

  toggleConfirmation() {
    this.sounds.confirmation = !this.sounds.confirmation;
    localStorage.setItem('confirmationSound', this.sounds.confirmation.toString());
  }

  constructor(
    public b: BlockService
  ) {
   }


  ngOnInit() {
    const confSound = localStorage.getItem('confirmationSound');
    this.sounds.confirmation = (confSound === 'true');

    this.b.connect();
    this.b.newBlock$.subscribe(nb => {
      const bestDl = <Array<Array<number>>>nb['bestDeadlines'];
      this.lineChartLabels = bestDl.map(x => x[0]);
      this.lineChartData[0].data = bestDl.map(x => x[1]);
    });
    this.b.newConfirmation$.subscribe(nc => {
      if (this.lineChartLabels[this.lineChartLabels.length - 1] === this.b.newBlock.block) {
        console.log('vec ima');
      } else {
        console.log('treba push');
        this.lineChartLabels.push(this.b.newBlock.block);
        this.lineChartData[0].data.push(this.bestDeadline().deadlineNum);
        setTimeout(() => { }, 0);
      }
    });
  }

  round(n: number): number {
    return Math.round(n);
  }

  bestDeadline(): JSONS.NonceObject {
    const bestDl = Math.min(...this.b.nonces.map(x => x.deadlineNum));

    if (bestDl !== Infinity) {
      return this.b.nonces.filter(x => x.deadlineNum == bestDl)[0];
    } else {
      return null;
    }

  }


}
