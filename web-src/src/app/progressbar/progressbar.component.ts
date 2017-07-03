import { Component, Input, ViewEncapsulation } from '@angular/core';
import { NgClass, NgStyle } from '@angular/common';

import { ProgressDirective } from './progress.directive';
import { BarComponent } from './bar.component';

@Component({
  selector: 'progressbar, [progressbar]',
  template: `
    <div appProgressBar [animate]="animate" [max]="max">
      <app-bar [type]="type" [value]="value">
          <ng-content></ng-content>
      </app-bar>
    </div>
  `,
  encapsulation: ViewEncapsulation.None
}
)
export class ProgressbarComponent {
  @Input() public animate: boolean;
  @Input() public max: number;
  @Input() public type: string;
  @Input() public value: number;
}
