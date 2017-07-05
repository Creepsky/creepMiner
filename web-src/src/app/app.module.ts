import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterModule } from '@angular/router';

import { ChartsModule } from 'ng2-charts';

import { AppComponent } from './app.component';
import { PlotFilesComponent } from './plot-files/plot-files.component';
import { SettingsComponent } from './settings/settings.component';
import { StatusComponent } from './status/status.component';

import { BlockService } from './block.service';
import { ProgressDirective } from './progressbar/progress.directive';
import { BarComponent } from './progressbar/bar.component';
import { ProgressbarComponent } from './progressbar/progressbar.component';

@NgModule({
  declarations: [
    AppComponent,
    PlotFilesComponent,
    SettingsComponent,
    StatusComponent,
    ProgressDirective,
    BarComponent,
    ProgressbarComponent
  ],
  imports: [
    BrowserModule,
    CommonModule,
    ChartsModule,
    RouterModule.forRoot([
      {
        path: 'status',
        component: StatusComponent
      },
      {
        path: 'plotfiles',
        component: PlotFilesComponent
      }, {
        path: 'settings',
        component: SettingsComponent
      },
      {
        path: '',
        redirectTo: '/status',
        pathMatch: 'full'
      },
    ])
  ],
  providers: [BlockService],
  bootstrap: [AppComponent]
})
export class AppModule { }
