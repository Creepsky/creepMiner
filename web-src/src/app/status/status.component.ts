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




}
