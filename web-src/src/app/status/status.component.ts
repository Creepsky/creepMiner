import { Component, OnInit } from '@angular/core';
import { BlockService } from '../block.service';

@Component({
  selector: 'app-status',
  templateUrl: './status.component.html',
  styleUrls: ['./status.component.css']
})
export class StatusComponent implements OnInit {



  constructor(
    public b: BlockService
  ) { }


  ngOnInit() {
    this.b.connectBlock();
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
