import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router';

import { AppComponent } from './app.component';
import { PlotFilesComponent } from './plot-files/plot-files.component';
import { SettingsComponent } from './settings/settings.component';
import { StatusComponent } from './status/status.component';

import { BlockService } from './block.service';

@NgModule({
  declarations: [
    AppComponent,
    PlotFilesComponent,
    SettingsComponent,
    StatusComponent
  ],
  imports: [
    BrowserModule,
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
