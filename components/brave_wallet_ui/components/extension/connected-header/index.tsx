import * as React from 'react'

// Styled Components
import {
  HeaderTitle,
  HeaderWrapper,
  ActionIcon,
  ExpandIcon
} from './style'

import { PanelTypes } from '../../../constants/types'

export interface Props {
  action: (path: PanelTypes) => void
}

export default class ConnectedHeader extends React.PureComponent<Props> {

  navigate = (path: PanelTypes) => () => {
    this.props.action(path)
  }

  render () {
    return (
      <HeaderWrapper>
        <ExpandIcon onClick={this.navigate('expanded')} />
        <HeaderTitle>Brave Web 3 Connect</HeaderTitle>
        <ActionIcon onClick={this.navigate('settings')} />
      </HeaderWrapper>
    )
  }
}
