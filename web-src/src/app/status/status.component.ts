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
    responsive: true
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

  constructor(
    public b: BlockService
  ) { }


  ngOnInit() {
    this.b.connectBlock();
    this.b.newBlock$.subscribe(nb => {
      const bestDl = <Array<Array<number>>>nb['bestDeadlines'];
      this.lineChartLabels = bestDl.map(x => x[0]);
      this.lineChartData[0].data = bestDl.map(x => x[1]);
    })
  }

  round(n: number): number {
    return Math.round(n);
  }

  bestDeadline(): string {
    const bestDl = Math.min(...this.b.nonces.map(x => x.deadlineNum));

    if (bestDl !== Infinity) {
      return this.b.nonces.filter(x => x.deadlineNum == bestDl)[0].deadline;
    } else {
      return '/';
    }

  }


}
