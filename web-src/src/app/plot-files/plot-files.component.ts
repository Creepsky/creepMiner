import { Component, OnInit } from '@angular/core';
import { BlockService } from '../block.service';


@Component({
  selector: 'app-plot-files',
  templateUrl: './plot-files.component.html',
  styleUrls: ['./plot-files.component.css']
})
export class PlotFilesComponent implements OnInit {

  constructor(
    public b: BlockService
  ) { }

  ngOnInit() {
    this.b.connect();
  }

}
